#!/usr/bin/env python3
"""Fetch the exact full Linux source and apply reviewable product adaptations.

Source preparation/auditing is allowed locally. Compilation belongs to Actions.
The donor is never merged over this repository's Windows/macOS/native sources.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import tarfile
import urllib.request

HERE = Path(__file__).resolve().parent
LOCK = json.loads((HERE / 'upstream.json').read_text())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def download(url, path, expected):
    if not path.exists() or digest(path) != expected:
        with urllib.request.urlopen(url, timeout=120) as response, path.open('wb') as output:
            shutil.copyfileobj(response, output)
    if digest(path) != expected:
        raise ValueError('SHA256 mismatch: ' + str(path))


def prepare(destination, archive):
    download(LOCK['archive'], archive, LOCK['sha256'])
    if destination.exists():
        raise ValueError('Use an empty source destination: ' + str(destination))
    destination.mkdir(parents=True)
    with tarfile.open(archive) as source:
        source.extractall(destination, filter='data')
    trees = list(destination.iterdir())
    if len(trees) != 1:
        raise ValueError('Unexpected upstream archive root')
    root = trees[0]
    changes = {}

    def edit(name, transform):
        path = root / name
        before = path.read_text()
        after = transform(before)
        if before == after:
            raise ValueError('Adaptation changed no source: ' + name)
        # A donor VERIFY claim cannot certify an adapted source body.
        after = re.sub(r'^// VERIFY:.*\n', '', after, flags=re.M)
        prefix = '#' if path.name == 'CMakeLists.txt' else '//'
        after = prefix + ' RovxBot adaptation of pinned 12340 source; see source-audit.json.\n' + after
        changes[name] = dict(upstream_sha256=digest(path))
        path.write_text(after)
        changes[name]['adapted_sha256'] = digest(path)

    def replace(text, before, after):
        if text.count(before) != 1:
            raise ValueError('Expected exactly one adaptation context: ' + before[:100])
        return text.replace(before, after)

    def function(text, signature, body):
        start = text.index(signature)
        opening = text.index('{', start)
        # Used only for the simple bodies below, which contain no brace literals.
        depth, end = 1, opening + 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        return text[:opening + 1] + '\n' + body + '\n}' + text[end:]

    header = '#include "RovNativePolicy.h"\n'
    def glue(text):
        start = text.index('// -- ', text.index('static bool AutoLoginGlueScreen') - 200)
        end = text.index('int32_t CGlueMgr::Idle', start)
        # Remove the entire donor credential-file/autologin/agreement automation.
        text = text[:start] + text[end:]
        text = replace(text, '    MaybeAutoLogin();\n', '')
        text = replace(text, '    DeleteInterfaceFiles();', '    // Original installation is read-only; no interface cleanup.')
        text = replace(text, '            CGVideoOptions::InstallExtendedViewUiPatch();', '            RovNative::GlueEnd();')
        text = replace(text, '            FrameXML_FreeHashNodes();', '            RovNative::GlueBegin();\n            RovNative::GlueVisit("Interface\\\\GlueXML\\\\GlueXML.toc");\n            FrameXML_FreeHashNodes();')
        text = replace(text, '            FrameXML_CreateFrames("Interface\\\\GlueXML\\\\GlueXML.toc", nullptr, &md5, &status);',
                       '            if (!FrameXML_CreateFrames("Interface\\\\GlueXML\\\\GlueXML.toc", nullptr, &md5, &status) || status.m_highestSeverity >= STATUS_ERROR) std::abort();')
        text = replace(text, '            if (memcmp(expectedDigest, digest, sizeof(digest))) {\n                break;\n            }',
                       '            if (memcmp(expectedDigest, digest, sizeof(digest))) {\n                std::fprintf(stderr, "Original post-creation GlueXML digest mismatch\\n");\n                std::abort();\n            }')
        signature = 'void CGlueMgr::LoginServerLogin(const char* username, char* password) {'
        text = replace(text, signature, signature + '\n    if (!RovNative::TestServerEnabled()) {\n        FrameScript_SignalEvent(GLUE_EVENT_OPEN_STATUS_DIALOG, "%s%s", "OKAY", RovNative::IntegrityUnavailable);\n        return;\n    }')
        text = replace(text, '!CGlueMgr::m_scandllOkayToLogIn || !CGlueMgr::m_acceptedTOS',
                       '(!CGlueMgr::m_scandllOkayToLogIn && !RovNative::TestServerEnabled()) || !CGlueMgr::m_acceptedTOS')
        return header + text + '\nextern "C" int RovNativeGlueProfile(int runtime) { return runtime ? RovNative::TestServerEnabled() : bool(WOWDECOMP_ISOLATED_SERVER_BUILD); }\n'
    edit('WoW/Source/Glue/CGlueMgr.cpp', glue)

    def client(text):
        for value in ['locale', 'record']:
            text = replace(text, '*reinterpret_cast<const uint32_t*>(' + value + ')',
                           'RovNative::PackedAttribute<uint32_t>(reinterpret_cast<const uint8_t*>(' + value + '))')
        text = function(text, 'void SetPaths()', '''    SFile::DisableSFileCheckDisk();
    SFile::EnableDirectAccess(0);
    const char* state = std::getenv("WOWDECOMP_SETTINGS");
    if (!state || !*state || !OsSetCurrentDirectory(state)) {
        std::fprintf(stderr, "Launch through the packaged application: external settings root required\\n");
        std::abort();
    }
    SFile::SetBasePath(state);
    s_unk_B2FA10 = 0;
    SFile::SetDataPath(s_dataDirectories[0]);''')
        # Do not read credentials from env, disk or command line.
        start = text.index('    {\n        const char* autoLogin = CmdLineGetString(CMD_AUTOLOGIN);')
        end = text.index('\n    }', start) + len('\n    }')
        text = text[:start] + text[end:]
        # OpenArchives runs on the main thread before any job dispatch. Retire
        # the idle pool on this specific failed-startup exit before std::exit
        # destroys its joinable std::thread owners. Retain the real failure.
        text = replace(text, '''                OsGuiMessageBox(OsGuiGetWindow(OSGUI_WINDOW_TYPE_FRONT), OSGUI_MESSAGE_STYLE_OK, message, title);
                StormExit(SErrGetLastError());''', '''                const int32_t startupError = SErrGetLastError();
                std::fprintf(stderr, "%s\\n", message);
                OsGuiMessageBox(OsGuiGetWindow(OSGUI_WINDOW_TYPE_FRONT), OSGUI_MESSAGE_STYLE_OK, message, title);
                Job::Shutdown();
                RovNative::failedStartupCleanup = true;
                CVar::Destroy();
                ConsoleCommandDestroy();
                SCmdDestroy();
                // Only the published prefix (including this null failed slot)
                // is initialized; the reserved tail is not an archive owner.
                s_archiveHandles.SetCount(slot + 1);
                CloseAllArchives();
                CStringManager::DestroyManager();
                StormExit(startupError ? startupError : 1);''')
        text = '#include "Engine/Source/Base/Job/JobSystem.h"\n#include "Engine/Source/Base/RCString.h"\n' + text
        text = replace(text,
                       '    qsort(this->m_files.Ptr(), count, sizeof(char*), ForEveryRunOnceWTF::FileNameCompare);',
                       '    if (count > 1) qsort(this->m_files.Ptr(), count, sizeof(char*), ForEveryRunOnceWTF::FileNameCompare);')
        return '#include "RovPackedAttributes.h"\n' + header + text
    edit('WoW/Source/Client.cpp', client)

    def grunt(text):
        text = replace(text, 'void GruntLogin::Logon(const char* loginServer, const char* portal) {',
                       'void GruntLogin::Logon(const char* loginServer, const char* portal) {\n    if (!RovNative::TestServerEnabled() || !RovNative::ExplicitEndpoint(loginServer)) {\n        this->m_loginResponse->UpdateLoginStatus(LOGIN_STATE_FAILED, LOGIN_FAILED, RovNative::IntegrityUnavailable, 0);\n        return;\n    }')
        text = function(text, 'void GruntLogin::GetVersionProof(', '''    if (!RovNative::TestServerEnabled() || this->IsReconnect()) {
        this->m_loginResponse->UpdateLoginStatus(LOGIN_STATE_FAILED, LOGIN_FAILED, RovNative::IntegrityUnavailable, 0);
        return;
    }
    // Explicit isolated-server profile, matching our existing native profile.
    // This is not a qualified executable proof. StrictVersionCheck must already
    // be disabled on the operator's isolated server; this client changes no policy.
    uint8_t unavailableExecutableProof[20] = {};
    this->ProveVersion(unavailableExecutableProof);''')
        text = function(text, 'void GruntLogin::Reconnect()', '''    this->m_loginResponse->UpdateLoginStatus(LOGIN_STATE_FAILED, LOGIN_FAILED, "Native reconnect integrity is unavailable", 0);''')
        return header + text + '\nextern "C" int RovNativeLoginProfile(int runtime) { return runtime ? RovNative::TestServerEnabled() : bool(WOWDECOMP_ISOLATED_SERVER_BUILD); }\n'
    edit('WoW/Source/WowServices/GruntLogin.cpp', grunt)
    edit('WoW/Source/WowServices/BattlenetLogin.cpp', lambda t: header + function(t, 'void BattlenetLogin::Logon(', '''    this->m_loginResponse->UpdateLoginStatus(LOGIN_STATE_FAILED, LOGIN_FAILED, "Native Battle.net login is unavailable", 0);'''))

    def platform(text):
        text = replace(text, 'return 0x783836;', 'return 0x783634; // x64')
        text = replace(text, 'return 0x57696E;', 'return 0x4C696E; // Lin')
        return text.replace('// ʕ •ᴥ•ʔ✿ Reports "Win" so 3.3.5a realm servers accept the logon challenge ✿ ʕ •ᴥ•ʔ',
                            '// Truthful Linux identity; never claim Windows executable integrity.')
    edit('Engine/Source/Os/Linux/System.cpp', platform)

    # #206: WORKER is a runtime owner, not a wire structure. POSIX events,
    # pthread locks and native pointers require their natural member alignment.
    # Keep the original packed Windows comparison layout and every other struct.
    def worker_alignment(text):
        text = replace(text, '#pragma pack(push, 1)',
                       '#if defined(_WIN32)\n#pragma pack(push, 1)\n#endif')
        return replace(text, '#pragma pack(pop)',
                       '#if defined(_WIN32)\n#pragma pack(pop)\n#endif')
    edit('WoW/Source/WowServices/WowConnectionNet.h', worker_alignment)

    def warden(text):
        return function(text, 'static int32_t ReceiveWardenData(', '''    std::fprintf(stderr, "Native Warden request is unsupported; disconnecting\\n");
    ClientServices::Connection()->Disconnect();
    return 0;''')
    edit('WoW/Source/Game/GameClient/Linux/WardenClient.cpp', lambda t: header + warden(t))
    def bots(text):
        return function(text, 'int32_t Packet_SMSG_CHECK_FOR_BOTS(', '''    std::fprintf(stderr, "Native server bot-check request is unsupported; disconnecting\\n");
    ClientServices::Connection()->Disconnect();
    return 0;''')
    edit('WoW/Source/Linux/BotCheck.cpp', lambda t: header + '#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"\n' + bots(t))

    def scan(text):
        text = function(text, 'void CGlueMgr::ScanDLLContinueAnyway()', '    // Displaying controls grants no native integrity approval.')
        text = function(text, 'bool CGlueMgr::IsScanDLLFinished()', '    return false; // Native ScanDLL is unavailable, including in the test profile.')
        text = function(text, 'void CGlueMgr::ScanDLLStart(', '''    CGlueMgr::m_scandllOkayToLogIn = 0;
    CGlueMgr::m_scandllStatus = 2;
    CGlueMgr::m_idleState = IDLE_14; // Original Idle publishes the error event.
    std::fprintf(stderr, "%s\\n", RovNative::IntegrityUnavailable);''')
        return header + text
    edit('WoW/Source/Glue/ScanDLLGlue.cpp', scan)

    # File creation/mutation is constrained to launcher-owned external roots,
    # including resolved symlinks. Original Data and addons may only be read.
    def files(text):
        text = replace(text, '    flags |= O_CLOEXEC;', '    if ((flags & (O_CREAT | O_TRUNC | O_WRONLY | O_RDWR)) && !RovNative::AllowedWrite(path)) return -1;\n    flags |= O_CLOEXEC;')
        for name in ['Move', 'Delete', 'CreateDirectory', 'RemoveDirectory']:
            signature = 'bool System_File::' + name + '(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {'
            check = '    if (!RovNative::AllowedWrite(parms->m_path)) return false;'
            if name == 'Move':
                check += '\n    if (!RovNative::AllowedWrite(parms->m_newpath)) return false;'
            text = replace(text, signature, signature + '\n' + check)
        signature = 'bool System_File::Copy(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {'
        text = replace(text, signature, signature + '\n    if (!RovNative::AllowedWrite(parms->m_newpath)) return false;')
        for name in ['SetAttributes', 'SetEOF']:
            signature = 'bool System_File::' + name + '(Blizzard::File::Filesystem* fs, System_File::Stacked::FileParms* parms) {'
            text = replace(text, signature, signature + '\n    if (!parms->m_stream || !RovNative::AllowedWrite(parms->m_stream->m_info.m_path)) return false;')
        # Lua addons retain their original paths; do not redirect to WASM addons.
        text = replace(text, 'static bool RewriteWasmAddonPath(const char* path, char* out, size_t outLen) {',
                       'static bool RewriteWasmAddonPath(const char* path, char* out, size_t outLen) {\n    return false; // Optional WASM is disabled in the baseline.\n')
        return header + text
    edit('Engine/Source/BlizzardCore/Source/System/File/Linux/Filestack.cpp', files)
    edit('Engine/Source/FrameXML/FrameXML.cpp', lambda t: header + replace(t, 'XMLTree* FrameXML_LoadXML(const char* filename, MD5_CTX* md5, CStatus* status) {',
         'XMLTree* FrameXML_LoadXML(const char* filename, MD5_CTX* md5, CStatus* status) {\n    RovNative::GlueVisit(filename);'))
    edit('Engine/Source/FrameScript/FrameScript.cpp', lambda t: header + replace(t, 'int32_t FrameScript_ExecuteFile(const char* filename, const char* wrapper, MD5_CTX* md5, CStatus* status) {',
         'int32_t FrameScript_ExecuteFile(const char* filename, const char* wrapper, MD5_CTX* md5, CStatus* status) {\n    if (md5) RovNative::GlueVisit(filename);'))

    # Keep SDL on the actual main thread with adequate stack; ucontext is not
    # compatible with reliable ASan stack tracking. No sanitizer is suppressed.
    def entry(text):
        text = function(text, 'int main(int argc, char** argv)', '''    if (!std::getenv("WOWDECOMP_SETTINGS")) {
        std::fprintf(stderr, "Use the packaged WoW application launcher\\n");
        return 1;
    }
    rlimit limit{};
    if (getrlimit(RLIMIT_STACK, &limit)) return 1;
    constexpr rlim_t minimum = 16 * 1024 * 1024;
    if (limit.rlim_cur < minimum) {
        if (limit.rlim_max != RLIM_INFINITY && limit.rlim_max < minimum) return 1;
        limit.rlim_cur = minimum;
        if (setrlimit(RLIMIT_STACK, &limit)) return 1;
    }
    std::fprintf(stderr, "native-login-profile login_build=%d glue_build=%d login_active=%d glue_active=%d\\n",
        RovNativeLoginProfile(0), RovNativeGlueProfile(0), RovNativeLoginProfile(1), RovNativeGlueProfile(1));
    StormRtlInitialize();
    return ClientMain();''')
        text = replace(text, 'int main(int argc, char** argv) {', '''int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "--native-profile")) {
        RovNative::CheckAllocationRoutes();
        std::printf("{\\"login_build\\":%d,\\"glue_build\\":%d,\\"login_active\\":%d,\\"glue_active\\":%d}\\n",
            RovNativeLoginProfile(0), RovNativeGlueProfile(0), RovNativeLoginProfile(1), RovNativeGlueProfile(1));
        return 0;
    }''')
        return ('#include <sys/resource.h>\n#include <cstring>\n#include "RovNativePolicy.h"\n'
                'extern "C" int RovNativeLoginProfile(int runtime);\nextern "C" int RovNativeGlueProfile(int runtime);\n' + text)
    edit('WoW/Source/Linux/WinMain.cpp', entry)

    # These donor declarations have no implementation or concrete construction.
    # Express the interfaces as abstract instead of inventing working services or
    # suppressing UBSan vptr checks to hide their missing RTTI key functions.
    edit('Engine/Source/Gx/Gx.h', lambda t: replace(replace(t,
         'virtual ~GxuDrawListState();', 'virtual ~GxuDrawListState() = default;'),
         'virtual ~GxuDrawListItem();', 'virtual ~GxuDrawListItem() = default;'))
    edit('Engine/Source/Gxu/GxuScene.h', lambda t: re.sub(
         r'(virtual (?:void|bool) (?:Unk00|Release|IsAlive|Update|Render)\([^;]*\));', r'\1 = 0;', t))
    def gui_interfaces(text):
        start = text.index('class COsControl {')
        end = text.index('class COsEditBox', start)
        section = text[start:end].replace('virtual ~COsControl();', 'virtual ~COsControl() = default;')
        section = re.sub(r'(virtual [^;\n]+\([^;\n]*\));', r'\1 = 0;', section)
        text = text[:start] + section + text[end:]
        return replace(text, 'virtual void SetSize(int32_t width, int32_t height);',
                       'virtual void SetSize(int32_t width, int32_t height) = 0;')
    edit('Engine/Source/Os/Gui.h', gui_interfaces)
    edit('Engine/Source/Storm/Source/IOUnit/IOFileUnit.h', lambda t: replace(t,
         'virtual ~IORefcount();', 'virtual ~IORefcount() = default;'))

    # Keep the original signed-relative model representation and reject wide
    # extents before the donor's page rounding/address additions can overflow.
    def near_memory(text):
        text = replace(text, '    uintptr_t granularity = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));',
                       '''    long page = sysconf(_SC_PAGESIZE);
    if (!anchor || page <= 0) return nullptr;
    uintptr_t granularity = static_cast<uintptr_t>(page);
    if ((granularity & (granularity - 1)) || size > INT32_MAX - s_headerSize - granularity ||
        reinterpret_cast<uintptr_t>(anchor) > UINTPTR_MAX - s_nearRange) return nullptr;''')
        return text
    edit('Engine/Source/Os/Linux/OsMemory.cpp', near_memory)
    def sound_angle(text):
        text = function(text, 'static FMOD_INLINE int FMOD_Cart2Angle(', '''    // Preserve the fixed-point approximation without signed left-shift UB.
    // Public int inputs fit every intermediate in int64_t, including INT_MIN.
    const int64_t sx = int64_t(x) * 1024;
    const int64_t sy = int64_t(y) * 1024;
    if (!sx && !sy) return 0;
    const int64_t pi = int(3.1415926535897932384626433832795f * 1024.0f);
    const int64_t coefficient = pi / 4;
    const int64_t abs_y = (sy < 0 ? -sy : sy) + 1;
    const int64_t ratio = sx >= 0 ? (sx - abs_y) / ((sx + abs_y) >> 10)
                                 : (sx + abs_y) / ((abs_y - sx) >> 10);
    int64_t angle = (sx >= 0 ? coefficient : 3 * coefficient) - ((coefficient * ratio) >> 10);
    if (sy < 0) angle = -angle;
    angle = angle * 180 / pi;
    return int(angle < 0 ? angle + 360 : angle >= 360 ? angle - 360 : angle);''')
        return '#include <cstdint>\n' + text
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_3d.h', sound_angle)
    # #186: a larger allocation does not construct the derived ALSA object.
    # Keep the FMOD pool and zeroed state, but begin the actual callback owner's
    # lifetime, as the factory already does for the built-in OpenAL output.
    def alsa_factory(text):
        text = replace(text, '#include "fmod_output_polled.h"',
                       '#include "fmod_output_polled.h"\n#ifdef FMOD_SUPPORT_ALSA\n#include "fmod_output_alsa.h"\n#endif')
        return replace(text, '    if (outputdesc->polling)\n', '''#ifdef FMOD_SUPPORT_ALSA
    if (outputdesc->mType == FMOD_OUTPUTTYPE_ALSA)
    {
        void *storage = FMOD_Memory_Calloc(sizeof(OutputALSA));
        newoutput = storage ? new (storage) OutputALSA : 0;
    }
    else
#endif
    if (outputdesc->polling)
''')
    # #188/#198/#200: allocating a callback owner's size and constructing
    # DSPFilter does not begin the concrete registered callback-owner lifetime.
    def filter_factory(text):
        text = replace(text, '#include "fmod_dsp_oscillator.h"',
                       '#include "fmod_dsp_oscillator.h"\n#include "fmod_dsp_lowpass_simple.h"\n#include "fmod_dsp_pitchshift.h"\n#include "fmod_dsp_chorus.h"\n#include "fmod_dsp_echo.h"')
        text = replace(text, '                new (newdsp) DSPFilter;', '''#ifdef FMOD_SUPPORT_LOWPASS_SIMPLE
                if (dspdesc->mType == FMOD_DSP_TYPE_LOWPASS_SIMPLE)
                {
                    new (newdsp) DSPLowPassSimple;
                }
                else
#endif
#ifdef FMOD_SUPPORT_PITCHSHIFT
                if (dspdesc->mType == FMOD_DSP_TYPE_PITCHSHIFT)
                {
                    new (newdsp) DSPPitchShift;
                }
                else
#endif
#ifdef FMOD_SUPPORT_CHORUS
                if (dspdesc->mType == FMOD_DSP_TYPE_CHORUS)
                {
                    new (newdsp) DSPChorus;
                }
                else
#endif
#ifdef FMOD_SUPPORT_ECHO
                if (dspdesc->mType == FMOD_DSP_TYPE_ECHO)
                {
                    new (newdsp) DSPEcho;
                }
                else
#endif
                {
                    new (newdsp) DSPFilter;
                }''')
        text = replace(text, '                size = size < sizeof(DSPFilter) ? sizeof(DSPFilter) : size;', '''#ifdef FMOD_SUPPORT_LOWPASS_SIMPLE
                if (dspdesc->mType == FMOD_DSP_TYPE_LOWPASS_SIMPLE)
                {
                    size = size < sizeof(DSPLowPassSimple) ? sizeof(DSPLowPassSimple) : size;
                }
#endif
#ifdef FMOD_SUPPORT_PITCHSHIFT
                if (dspdesc->mType == FMOD_DSP_TYPE_PITCHSHIFT)
                {
                    size = size < sizeof(DSPPitchShift) ? sizeof(DSPPitchShift) : size;
                }
#endif
#ifdef FMOD_SUPPORT_CHORUS
                if (dspdesc->mType == FMOD_DSP_TYPE_CHORUS)
                {
                    size = size < sizeof(DSPChorus) ? sizeof(DSPChorus) : size;
                }
#endif
#ifdef FMOD_SUPPORT_ECHO
                if (dspdesc->mType == FMOD_DSP_TYPE_ECHO)
                {
                    size = size < sizeof(DSPEcho) ? sizeof(DSPEcho) : size;
                }
#endif
                size = size < sizeof(DSPFilter) ? sizeof(DSPFilter) : size;''')
        # #200: these four recursive filters keep coefficients/history inline,
        # register no release callback and have trivial destruction. Only begin
        # their lifetime here; preserve each original create/reset/parameter path.
        construction = ''
        for feature, kind, owner, header in [
            ('LOWPASS', 'LOWPASS', 'DSPLowPass', 'lowpass'),
            ('LOWPASS2', 'ITLOWPASS', 'DSPLowPass2', 'lowpass2'),
            ('HIGHPASS', 'HIGHPASS', 'DSPHighPass', 'highpass'),
            ('PARAMEQ', 'PARAMEQ', 'DSPParamEq', 'parameq'),
        ]:
            text = replace(text, '#include "fmod_dsp_oscillator.h"',
                           '#include "fmod_dsp_oscillator.h"\n#include "fmod_dsp_' + header + '.h"')
            text = replace(text, '                size = size < sizeof(DSPFilter) ? sizeof(DSPFilter) : size;',
                           f'''#ifdef FMOD_SUPPORT_{feature}
                if (dspdesc->mType == FMOD_DSP_TYPE_{kind})
                {{
                    size = size < sizeof({owner}) ? sizeof({owner}) : size;
                }}
#endif
                size = size < sizeof(DSPFilter) ? sizeof(DSPFilter) : size;''')
            construction += f'''#ifdef FMOD_SUPPORT_{feature}
                if (dspdesc->mType == FMOD_DSP_TYPE_{kind})
                {{
                    new (newdsp) {owner};
                }}
                else
#endif
'''
        text = replace(text, '                {\n                    new (newdsp) DSPFilter;\n                }',
                       construction + '                {\n                    new (newdsp) DSPFilter;\n                }')
        # These concrete types have no destructor-owned resources. The existing
        # alloc/create failure frees and DSPFilter::release end its lifetime by
        # releasing pool storage; preserve them, including release(false).
        return text
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_pluginfactory.cpp',
         lambda t: filter_factory(alsa_factory(t)))
    # #198: a later default-parameter failure can follow allocation of the
    # per-channel pitch-shift state. The factory frees only the object on error;
    # unwind these callback-owned resources here, before that trivial lifetime ends.
    def pitchshift_creation(text):
        start = text.index('FMOD_RESULT DSPPitchShift::createInternal()')
        end = text.index('FMOD_RESULT DSPPitchShift::releaseInternal()', start)
        creation = replace(text[start:end], '            return result;',
                           '            releaseInternal();\n            return result;')
        text = text[:start] + creation + text[end:]
        # At FFT sizes 2048/4096 the mirrored final interval has a fractional
        # table position below one. Its floor is zero, so the original -1
        # neighbour reads before vwin1024. Use the first valid table interval;
        # preserve every previously in-bounds interpolation and the FFT algorithm.
        return replace(text, '                k_a = (int)ktabpos;\n                float win_a',
                       '''                k_a = (int)ktabpos;
                if (k_a == 0)
                {
                    k_next = 1;
                }
                float win_a''')
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_dsp_pitchshift.cpp', pitchshift_creation)
    # #200: Chorus owns its delay buffer through releaseInternal, rather than a
    # destructor. Unwind it if any default parameter fails after allocation;
    # createDSP frees the object storage without invoking a release callback.
    def chorus_creation(text):
        start = text.index('FMOD_RESULT DSPChorus::createInternal()')
        end = text.index('FMOD_RESULT DSPChorus::releaseInternal()', start)
        creation = replace(text[start:end], '            return result;',
                           '            releaseInternal();\n            return result;')
        return text[:start] + creation + text[end:]
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_dsp_chorus.cpp', chorus_creation)
    # #200: Echo's default delay allocates its callback-owned buffer before
    # subsequent parameters can fail. Match its existing release ownership when
    # unwinding a failed create, before the factory frees the trivial object.
    def echo_creation(text):
        start = text.index('FMOD_RESULT DSPEcho::createInternal()')
        end = text.index('FMOD_RESULT DSPEcho::releaseInternal()', start)
        creation = replace(text[start:end], '            return result;',
                           '            releaseInternal();\n            return result;')
        return text[:start] + creation + text[end:]
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_dsp_echo.cpp', echo_creation)
    # #202: share the product's half-open parameter bounds in the pinned graph.
    def parameter_bounds(text):
        before = 'index < 0 || index > mDescription.numparameters'
        if text.count(before) != 2:
            raise ValueError('Expected the shared setter/getter parameter guards')
        return text.replace(before, 'index < 0 || index >= mDescription.numparameters')
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_dspi.cpp', parameter_bounds)
    edit('Engine/Source/SoundEngine/fmodex/linux/src/fmod_output_alsa.h', lambda t: replace(t,
         '        static FMOD_OUTPUT_DESCRIPTION_EX *getDescriptionEx();',
         '        static FMOD_OUTPUT_DESCRIPTION_EX *getDescriptionEx();\n\n        FMOD_RESULT              release() override;'))
    edit('Engine/Source/SoundEngine/fmodex/linux/src/fmod_output_alsa.cpp', lambda t: replace(t,
         'void *OutputALSA::writerThread(void *param)', '''// SystemI stops the mixer and calls close before release. End the concrete
// lifetime here, then return its original allocation to the same FMOD pool.
FMOD_RESULT OutputALSA::release()
{
    void *storage = this;
    this->~OutputALSA();
    FMOD_Memory_Free(storage);
    return FMOD_OK;
}

void *OutputALSA::writerThread(void *param)'''))
    def output_replacement(text):
        # Both pre-init selection APIs must honor the concrete release route.
        # No callback/stop/close ordering is changed by output replacement.
        if text.count('        FMOD_Memory_Free(mOutput);') != 2:
            raise ValueError('Expected two pre-init output replacement paths')
        text = text.replace('        FMOD_Memory_Free(mOutput);', '        mOutput->release();')
        return replace(text, '        mOutput->release();\n    }',
                       '        mOutput->release();\n        mOutput = 0;\n    }')
    edit('Engine/Source/SoundEngine/fmodex/src/fmod_systemi.cpp', output_replacement)
    def allocation_routes(text):
        text = '#include <new>\n#include <limits>\n' + text
        text = replace(text, '    size_t size = (bytes + 7) & ~static_cast<size_t>(7);\n    void* ptr =',
                       '''    if (bytes > std::numeric_limits<size_t>::max() - 7) {
        FatalError(bytes, filename, linenumber);
        return nullptr;
    }
    size_t size = ((bytes ? bytes : 1) + 7) & ~static_cast<size_t>(7);
    void* ptr =''')
        text = replace(text, '    return SMemAlloc(bytes, "new", SLOG_FUNCTION, 0);',
                       '    void* ptr = SMemAlloc(bytes, "new", SLOG_FUNCTION, 0);\n    if (!ptr) throw std::bad_alloc();\n    return ptr;')
        text += '''
// Complete C++17 allocation/deallocation pairs. The original replacement new
// uses malloc through SMem; libasan/libstdc++ sized/array delete must not take
// ownership of those pointers. No mismatch detector is disabled.
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* ptr, size_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, size_t) noexcept { ::operator delete[](ptr); }
void* operator new(size_t bytes, std::align_val_t alignment) {
    void* ptr = nullptr;
    const size_t requested = static_cast<size_t>(alignment);
    if (!requested || (requested & (requested - 1))) throw std::bad_alloc();
    // Explicit aligned-new callers (including LLVM) may request alignment 1,
    // 2 or 4. POSIX requires at least sizeof(void*); greater alignment is valid.
    const size_t nativeAlignment = requested < sizeof(void*) ? sizeof(void*) : requested;
    if (posix_memalign(&ptr, nativeAlignment, bytes ? bytes : 1)) throw std::bad_alloc();
    return ptr;
}
void* operator new[](size_t bytes, std::align_val_t alignment) { return ::operator new(bytes, alignment); }
void operator delete(void* ptr, std::align_val_t) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { ::operator delete[](ptr); }
void operator delete(void* ptr, size_t, std::align_val_t alignment) noexcept { ::operator delete(ptr, alignment); }
void operator delete[](void* ptr, size_t, std::align_val_t alignment) noexcept { ::operator delete[](ptr, alignment); }
void* operator new(size_t bytes, const std::nothrow_t&) noexcept {
    try { return ::operator new(bytes); } catch (...) { return nullptr; }
}
void* operator new[](size_t bytes, const std::nothrow_t&) noexcept {
    try { return ::operator new[](bytes); } catch (...) { return nullptr; }
}
void operator delete(void* ptr, const std::nothrow_t&) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { ::operator delete[](ptr); }
void* operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    try { return ::operator new(bytes, alignment); } catch (...) { return nullptr; }
}
void* operator new[](size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    try { return ::operator new[](bytes, alignment); } catch (...) { return nullptr; }
}
void operator delete(void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept { ::operator delete(ptr, alignment); }
void operator delete[](void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept { ::operator delete[](ptr, alignment); }
'''
        return text
    edit('Engine/Source/Storm/Source/SMem.cpp', allocation_routes)
    # Reuse the product's native CStringMemory correction. Donor entries still
    # round to four bytes, leaving their native pointer-chain links misaligned.
    # The actual CVar/missing-data startup check exercises these pooled strings.
    edit('Engine/Source/Base/RCString.cpp', lambda t: replace(t,
         '    uint32_t bytes = (SStrLen(str) + sizeof(char*) + 4) & ~3;',
         '    size_t bytes = (SStrLen(str) + sizeof(char*) + alignof(char*)) & ~(alignof(char*) - 1);'))
    # Missing archives terminate before engine/Event/Base initialization. Release
    # only the initialized settings owners and preserve the existing config.
    edit('Engine/Source/ConsoleBase/ConsoleVar.cpp', lambda t: header + replace(t,
         '    CVar::m_initialized = false;\n    CVar::SaveAll();',
         '    CVar::m_initialized = false;\n    if (!RovNative::failedStartupCleanup) CVar::SaveAll();'))
    def packed_attributes(text):
        text = replace(text, 'size > (count << 9) + 8', 'uint64_t(size) > uint64_t(count) * 512 + 8')
        text = replace(text, '    const uint32_t* header = reinterpret_cast<const uint32_t*>(data.m_data);\n    uint32_t flags = header[1];',
                       '    uint32_t flags = RovNative::PackedAttribute<uint32_t>(data.m_data + 4);')
        start = text.index('    uint32_t remaining = size - 8;', text.index('bool System_SFile::ARCHIVEREC::ReadAdditionalAttributes()'))
        end = text.index('    if (flags & 1) {\n        for', start)
        text = text[:start] + '''    const uint8_t* p = data.m_data + 8;
    if (!RovNative::ValidAttributeSize(count, size, flags)) return false;

''' + text[end:]
        for value in ['uint32_t', 'uint64_t', 'Blizzard::Util::Digest<16>']:
            text = replace(text, '*reinterpret_cast<const ' + value + '*>(p)', 'RovNative::PackedAttribute<' + value + '>(p)')
        return '#include "RovPackedAttributes.h"\n' + text
    edit('Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.cpp', packed_attributes)
    # Reuse the product native ftstream.h correction for the reached BE long
    # decoder (#177): signed high-byte scaling fits FT_Long even at -128.
    # Preserve the other bytes, pointer advance and existing unsigned casts.
    def freetype_stream(text):
        text = replace(text, '( (long)(signed   char)buffer[-4] << 24 )',
                       '( (long)(signed   char)buffer[-4] * 0x1000000L )')
        # #180: the same native correction for both short byte orders. The
        # signed high byte times 256 fits int/short even at -128; retain the
        # short cast, low byte, unsigned wrappers and two-byte cursor advance.
        for high_byte in [-2, -1]:
            text = replace(text, f'( (signed   char)buffer[{high_byte}] << 8 )',
                           f'( (signed   char)buffer[{high_byte}] * 0x100 )')
        return text
    edit('ThirdParty/freetype/include/freetype/internal/ftstream.h', freetype_stream)
    # #184: check the null list terminator before forming &o->uv. Reuse the
    # product native correction; keep find/close ordering and GC logic intact.
    edit('Engine/Source/lua-5.1/src/lstate.h', lambda t: replace(t,
         '#define ngcotouv(o) \\\n\tcheck_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))',
         '#define ngcotouv(o) ((o) == NULL ? NULL : gco2uv(o))'))
    # #182: cast before shifting, as in the product native path. Keep both
    # rand() calls, low-word truncation and the random-table initialization.
    edit('Engine/Source/Model2/M2Cache.cpp', lambda t: replace(t,
         '    uint32_t high = rand() << 16;',
         '    uint32_t high = static_cast<uint32_t>(rand()) << 16;'))
    # #190: the device does not enable VK_AMD_device_coherent_memory. Such
    # types may still be advertised and must not enter preferred or fallback
    # allocation paths (VUID-vkAllocateMemory-deviceCoherentMemory-02790).
    def vulkan_memory_types(text):
        text = replace(text, 'void VkContext::ChooseHeaps() {', '''// FinishCreateDevice does not enable VK_AMD_device_coherent_memory.
static bool MemoryTypeEnabled(const VkMemoryType& type) {
    return !(type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD);
}

void VkContext::ChooseHeaps() {''')
        text = replace(text, '        VkMemoryPropertyFlags flags = type.propertyFlags;\n',
                       '''        VkMemoryPropertyFlags flags = type.propertyFlags;
        if (!MemoryTypeEnabled(type)) {
            continue;
        }
''')
        text = replace(text, '        if ((typeBits & (1u << i)) && (this->m_memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {',
                       '        if ((typeBits & (1u << i)) && MemoryTypeEnabled(this->m_memoryProperties.memoryTypes[i]) && (this->m_memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {')
        text = replace(text, '            if ((requirements.memoryTypeBits & (1u << i)) &&\n',
                       '            if ((requirements.memoryTypeBits & (1u << i)) && MemoryTypeEnabled(type) &&\n')
        return text
    edit('Engine/Source/Gx/CGxDeviceVulkan/VkContext.cpp', vulkan_memory_types)
    # #191: a depth-only view does not limit the image layout transition.
    # The device does not enable separateDepthStencilLayouts.
    device_path = 'Engine/Source/Gx/CGxDeviceVulkan/CGxDeviceVulkan.cpp'
    device_before = (root / device_path).read_text()
    edit(device_path, lambda t: replace(t,
         '        VkContext::Barrier(layoutCmd, this->m_offscreenDepth.image, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);',
         '''        const VkImageAspectFlags aspects =
            info.format == VK_FORMAT_D16_UNORM_S8_UINT ||
            info.format == VK_FORMAT_D24_UNORM_S8_UINT ||
            info.format == VK_FORMAT_D32_SFLOAT_S8_UINT
                ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                : VK_IMAGE_ASPECT_DEPTH_BIT;
        VkContext::Barrier(layoutCmd, this->m_offscreenDepth.image, aspects, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);'''))
    # #192: a merged character run binds its own compact mesh. Its last
    # element cannot stand in for the ordinary model's cached buffer state.
    scene_path = 'Engine/Source/Model2/CM2SceneRender.cpp'
    scene_before = (root / scene_path).read_text()
    edit(scene_path, lambda t: replace(t,
         '                if (run > 1 && DrawCharacterRun(this, elements, indices, i, run)) {\n',
         '''                if (run > 1 && DrawCharacterRun(this, elements, indices, i, run)) {
                    // The merged mesh has private index/vertex buffers. Force
                    // the next ordinary batch to reestablish its bindings.
                    this->m_curType = -1;
'''))
    scene = (root / scene_path).read_text()

    def source_function(text, signature):
        start = text.index(signature)
        opening = text.index('{', start)
        depth, end = 1, opening + 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        return text[start:end] + '\n'

    # Exercise the actual dispatcher, merged submission and ordinary binding
    # paths against recording services. The unchanged donor dispatcher is a
    # negative control for the exact reached stale-buffer range.
    dispatch = 'void CM2SceneRender::Draw(M2PASS '
    functions = ['void CM2SceneRender::SetBatchVertices(',
                 'void CM2SceneRender::DrawBatch()',
                 'static int32_t DrawCharacterRun(', dispatch]
    mesh_start = scene.index('struct CharacterMesh {')
    mesh_end = scene.index('static CharacterMesh* CharacterMeshFor(', mesh_start)
    (root / 'RovNativeM2Batch.inc').write_text(
        scene[mesh_start:mesh_end] + ''.join(source_function(scene, s) for s in functions))
    (root / 'RovNativeM2BatchBefore.inc').write_text(
        source_function(scene_before, dispatch).replace('CM2SceneRender::Draw(', 'CM2SceneRender::DrawBeforeFix(', 1))
    # Compile the actual adapted selection and image-fallback implementations
    # in the focused hosted fixture, without linking the full Gx device.
    vk = (root / 'Engine/Source/Gx/CGxDeviceVulkan/VkContext.cpp').read_text()
    begin = vk.index('static bool MemoryTypeEnabled(')
    end = vk.index('int32_t VkContext::AcquireBuffer(', begin)
    image_begin = vk.index('int32_t VkContext::CreateImage(')
    image_end = vk.index('VkStaging VkContext::Stage(', image_begin)
    (root / 'RovNativeVulkanMemorySelect.inc').write_text(vk[begin:end] + vk[image_begin:image_end])
    device = (root / device_path).read_text()
    device_header = (root / 'Engine/Source/Gx/CGxDeviceVulkan/CGxDeviceVulkan.h').read_text()
    texture_start = device_header.index('struct VkTexture {')
    texture_end = device_header.index('\n};', texture_start) + len('\n};')
    (root / 'RovNativeVulkanFramebuffer.inc').write_text(
        device_header[texture_start:texture_end] + '\n' +
        source_function(vk, 'void VkContext::Barrier(') +
        source_function(device, 'VkFramebuffer CGxDeviceVulkan::ITexFramebuffer(') +
        source_function(device_before, 'VkFramebuffer CGxDeviceVulkan::ITexFramebuffer(').replace(
            'CGxDeviceVulkan::ITexFramebuffer(', 'CGxDeviceVulkan::ITexFramebufferBeforeFix(', 1))
    # Exercise the actual adapted statements with controlled rand() results;
    # extracting them avoids adding a production helper solely for this test.
    cache = (root / 'Engine/Source/Model2/M2Cache.cpp').read_text()
    seed_statements = re.findall(r'    uint32_t high = [^\n]+\n    CRndSeed seed\([^\n]+\n', cache)
    if len(seed_statements) != 1:
        raise ValueError('Expected exactly one M2 cache seed initialization')
    (root / 'RovNativeM2CacheSeed.inc').write_text(seed_statements[0])
    shutil.copy2(HERE / 'NativePolicy.h', root / 'RovNativePolicy.h')
    shutil.copy2(HERE / 'policy_check.cpp', root / 'RovNativePolicyCheck.cpp')
    shutil.copy2(HERE / 'near_check.cpp', root / 'RovNativeNearCheck.cpp')
    shutil.copy2(HERE / 'angle_check.cpp', root / 'RovNativeAngleCheck.cpp')
    shutil.copy2(HERE / 'PackedAttributes.h', root / 'RovPackedAttributes.h')
    shutil.copy2(HERE / 'attributes_check.cpp', root / 'RovNativeAttributesCheck.cpp')
    shutil.copy2(HERE / 'freetype_stream_check.cpp', root / 'RovNativeFreeTypeStreamCheck.cpp')
    shutil.copy2(HERE / 'm2_cache_seed_check.cpp', root / 'RovNativeM2CacheSeedCheck.cpp')
    shutil.copy2(HERE / 'lua_upvalue_check.cpp', root / 'RovNativeLuaUpvalueCheck.cpp')
    shutil.copy2(HERE / 'alsa_output_check.cpp', root / 'RovNativeALSAOutputCheck.cpp')
    shutil.copy2(HERE / 'lowpass_check.cpp', root / 'RovNativeLowPassCheck.cpp')
    shutil.copy2(HERE / 'pitchshift_check.cpp', root / 'RovNativePitchShiftCheck.cpp')
    shutil.copy2(HERE / 'chorus_check.cpp', root / 'RovNativeChorusCheck.cpp')
    shutil.copy2(HERE / 'echo_check.cpp', root / 'RovNativeEchoCheck.cpp')
    shutil.copy2(HERE / 'worker_check.cpp', root / 'RovNativeWorkerCheck.cpp')
    shutil.copy2(HERE / 'parameter_check.cpp', root / 'RovNativeParameterCheck.cpp')
    shutil.copy2(HERE / 'inline_filters_check.cpp', root / 'RovNativeInlineFiltersCheck.cpp')
    shutil.copy2(HERE / 'vulkan_memory_check.cpp', root / 'RovNativeVulkanMemoryCheck.cpp')
    shutil.copy2(HERE / 'vulkan_framebuffer_check.cpp', root / 'RovNativeVulkanFramebufferCheck.cpp')
    shutil.copy2(HERE / 'm2_batch_check.cpp', root / 'RovNativeM2BatchCheck.cpp')
    cmake = '''
# Separate full Linux graph; no product Windows/macOS or OpenGL target is changed.
if(NOT WOWDECOMP_LINUX OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    message(FATAL_ERROR "The adapted full client currently requires Linux x86-64")
endif()
if(WOWDECOMP_DISABLE_SIGNATURE_CHECKS)
    message(FATAL_ERROR "Original archive and interface signatures are mandatory")
endif()
add_executable(RovNativePolicyCheck RovNativePolicyCheck.cpp)
target_include_directories(RovNativePolicyCheck PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME rov_native_policy COMMAND RovNativePolicyCheck)
add_executable(RovNativeNearCheck RovNativeNearCheck.cpp Engine/Source/Os/Linux/OsMemory.cpp)
target_include_directories(RovNativeNearCheck PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME rov_native_near_memory COMMAND RovNativeNearCheck)
set_tests_properties(rov_native_policy rov_native_near_memory PROPERTIES TIMEOUT 45)
add_executable(RovNativeAngleCheck RovNativeAngleCheck.cpp)
target_link_libraries(RovNativeAngleCheck PRIVATE fmodex)
add_test(NAME rov_native_audio_angle COMMAND RovNativeAngleCheck)
set_tests_properties(rov_native_audio_angle PROPERTIES TIMEOUT 45)
add_executable(RovNativeAttributesCheck RovNativeAttributesCheck.cpp)
target_include_directories(RovNativeAttributesCheck PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME rov_native_packed_attributes COMMAND RovNativeAttributesCheck)
set_tests_properties(rov_native_packed_attributes PROPERTIES TIMEOUT 45)
add_executable(RovNativeFreeTypeStreamCheck RovNativeFreeTypeStreamCheck.cpp)
target_link_libraries(RovNativeFreeTypeStreamCheck PRIVATE freetype)
add_test(NAME rov_native_freetype_stream COMMAND RovNativeFreeTypeStreamCheck)
set_tests_properties(rov_native_freetype_stream PROPERTIES TIMEOUT 45)
add_executable(RovNativeM2CacheSeedCheck RovNativeM2CacheSeedCheck.cpp)
add_test(NAME rov_native_m2_cache_seed COMMAND RovNativeM2CacheSeedCheck)
set_tests_properties(rov_native_m2_cache_seed PROPERTIES TIMEOUT 45)
add_executable(RovNativeLuaUpvalueCheck RovNativeLuaUpvalueCheck.cpp)
target_link_libraries(RovNativeLuaUpvalueCheck PRIVATE "$<LINK_GROUP:RESCAN,lua,Base,Storm,Os,BlizzardCore>")
add_test(NAME rov_native_lua_upvalue COMMAND RovNativeLuaUpvalueCheck)
set_tests_properties(rov_native_lua_upvalue PROPERTIES TIMEOUT 45)
add_executable(RovNativeALSAOutputCheck RovNativeALSAOutputCheck.cpp)
target_include_directories(RovNativeALSAOutputCheck PRIVATE Engine/Source/SoundEngine/fmodex/linux/src)
target_link_libraries(RovNativeALSAOutputCheck PRIVATE fmodex)
# Failure injection/observation belongs only to the fixture. Successful calls
# use the real ALSA null PCM, ring buffer, writer thread and pthread join.
target_link_options(RovNativeALSAOutputCheck PRIVATE
    -Wl,--wrap=snd_pcm_set_params -Wl,--wrap=snd_pcm_close -Wl,--wrap=snd_pcm_drop
    -Wl,--wrap=snd_pcm_writei -Wl,--wrap=pthread_create -Wl,--wrap=pthread_join)
add_test(NAME rov_native_alsa_output COMMAND RovNativeALSAOutputCheck)
set_tests_properties(rov_native_alsa_output PROPERTIES TIMEOUT 45)
add_executable(RovNativeLowPassCheck RovNativeLowPassCheck.cpp)
target_link_libraries(RovNativeLowPassCheck PRIVATE fmodex)
add_test(NAME rov_native_lowpass COMMAND RovNativeLowPassCheck)
set_tests_properties(rov_native_lowpass PROPERTIES TIMEOUT 45)
add_executable(RovNativePitchShiftCheck RovNativePitchShiftCheck.cpp)
target_link_libraries(RovNativePitchShiftCheck PRIVATE fmodex)
add_test(NAME rov_native_pitchshift COMMAND RovNativePitchShiftCheck)
set_tests_properties(rov_native_pitchshift PROPERTIES TIMEOUT 45)
add_executable(RovNativeChorusCheck RovNativeChorusCheck.cpp)
target_link_libraries(RovNativeChorusCheck PRIVATE fmodex)
add_test(NAME rov_native_chorus COMMAND RovNativeChorusCheck)
set_tests_properties(rov_native_chorus PROPERTIES TIMEOUT 45)
add_executable(RovNativeEchoCheck RovNativeEchoCheck.cpp)
target_link_libraries(RovNativeEchoCheck PRIVATE fmodex)
add_test(NAME rov_native_echo COMMAND RovNativeEchoCheck)
set_tests_properties(rov_native_echo PROPERTIES TIMEOUT 45)
# Reuse actual full-client libraries and common sources; the fixture supplies
# main without running Client startup. No replacement transport or Storm owner.
file(GLOB worker_client_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/WoW/Source/*.cpp")
add_executable(RovNativeWorkerCheck RovNativeWorkerCheck.cpp ${worker_client_sources}
    Engine/Source/Perf/PerfLog.cpp)
get_target_property(worker_client_libraries WowClient LINK_LIBRARIES)
target_link_libraries(RovNativeWorkerCheck PRIVATE ${worker_client_libraries})
target_include_directories(RovNativeWorkerCheck PRIVATE ${CMAKE_SOURCE_DIR})
target_link_options(RovNativeWorkerCheck PRIVATE
    -Wl,--wrap=_Z11SSyncCreate9SSYNCTYPEiii -Wl,--wrap=pthread_create
    -Wl,--wrap=pthread_mutex_init -Wl,--wrap=pthread_mutex_destroy
    -Wl,--wrap=pipe2 -Wl,--wrap=poll)
add_test(NAME rov_native_network_worker COMMAND RovNativeWorkerCheck)
set_tests_properties(rov_native_network_worker PROPERTIES TIMEOUT 45)
add_executable(RovNativeParameterCheck RovNativeParameterCheck.cpp)
target_link_libraries(RovNativeParameterCheck PRIVATE fmodex)
add_test(NAME rov_native_parameters COMMAND RovNativeParameterCheck)
set_tests_properties(rov_native_parameters PROPERTIES TIMEOUT 45)
add_executable(RovNativeInlineFiltersCheck RovNativeInlineFiltersCheck.cpp)
target_link_libraries(RovNativeInlineFiltersCheck PRIVATE fmodex)
foreach(filter lowpass_full itlowpass highpass parameq)
    add_test(NAME rov_native_${filter} COMMAND RovNativeInlineFiltersCheck ${filter})
    set_tests_properties(rov_native_${filter} PROPERTIES TIMEOUT 45)
endforeach()
add_executable(RovNativeVulkanMemoryCheck RovNativeVulkanMemoryCheck.cpp)
target_include_directories(RovNativeVulkanMemoryCheck PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(RovNativeVulkanMemoryCheck PRIVATE volk)
add_test(NAME rov_native_vulkan_memory COMMAND RovNativeVulkanMemoryCheck)
set_tests_properties(rov_native_vulkan_memory PROPERTIES TIMEOUT 45)
add_executable(RovNativeVulkanFramebufferCheck RovNativeVulkanFramebufferCheck.cpp)
target_include_directories(RovNativeVulkanFramebufferCheck PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(RovNativeVulkanFramebufferCheck PRIVATE volk)
add_test(NAME rov_native_vulkan_framebuffer COMMAND RovNativeVulkanFramebufferCheck)
set_tests_properties(rov_native_vulkan_framebuffer PROPERTIES TIMEOUT 45)
add_executable(RovNativeM2BatchCheck RovNativeM2BatchCheck.cpp)
add_test(NAME rov_native_m2_batch COMMAND RovNativeM2BatchCheck)
set_tests_properties(rov_native_m2_batch PROPERTIES TIMEOUT 45)
'''
    early_profile = '''
# Must precede add_subdirectory: child directories snapshot these definitions.
option(WOWDECOMP_ISOLATED_SERVER_BUILD "Explicit isolated test server profile" OFF)
add_compile_definitions(WOWDECOMP_ISOLATED_SERVER_BUILD=$<BOOL:${WOWDECOMP_ISOLATED_SERVER_BUILD}>)
'''
    edit('CMakeLists.txt', lambda t: replace(t, 'project(wowdecomp CXX C)', 'project(wowdecomp CXX C)\n' + early_profile) + cmake)
    audit = dict(upstream=LOCK, adaptations=changes,
                 added={name: digest(root / name) for name in ['RovNativePolicy.h', 'RovNativePolicyCheck.cpp', 'RovNativeNearCheck.cpp', 'RovNativeAngleCheck.cpp', 'RovPackedAttributes.h', 'RovNativeAttributesCheck.cpp', 'RovNativeFreeTypeStreamCheck.cpp', 'RovNativeM2CacheSeedCheck.cpp', 'RovNativeM2CacheSeed.inc', 'RovNativeLuaUpvalueCheck.cpp', 'RovNativeALSAOutputCheck.cpp', 'RovNativeLowPassCheck.cpp', 'RovNativePitchShiftCheck.cpp', 'RovNativeChorusCheck.cpp', 'RovNativeEchoCheck.cpp', 'RovNativeInlineFiltersCheck.cpp', 'RovNativeParameterCheck.cpp', 'RovNativeWorkerCheck.cpp', 'RovNativeVulkanMemoryCheck.cpp', 'RovNativeVulkanMemorySelect.inc', 'RovNativeVulkanFramebufferCheck.cpp', 'RovNativeVulkanFramebuffer.inc', 'RovNativeM2BatchCheck.cpp', 'RovNativeM2Batch.inc', 'RovNativeM2BatchBefore.inc']})
    (root / 'source-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    return root


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.destination.resolve(), args.archive.resolve()))
