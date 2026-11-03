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
        return header + text
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
    StormRtlInitialize();
    return ClientMain();''')
        text = replace(text, 'int main(int argc, char** argv) {', '''int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "--native-profile")) {
        std::printf("{\\"login_build\\":%d,\\"glue_build\\":%d,\\"login_active\\":%d,\\"glue_active\\":%d}\\n",
            RovNativeLoginProfile(0), RovNativeGlueProfile(0), RovNativeLoginProfile(1), RovNativeGlueProfile(1));
        return 0;
    }''')
        return ('#include <sys/resource.h>\n#include <cstring>\n'
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
    shutil.copy2(HERE / 'NativePolicy.h', root / 'RovNativePolicy.h')
    shutil.copy2(HERE / 'policy_check.cpp', root / 'RovNativePolicyCheck.cpp')
    shutil.copy2(HERE / 'near_check.cpp', root / 'RovNativeNearCheck.cpp')
    shutil.copy2(HERE / 'angle_check.cpp', root / 'RovNativeAngleCheck.cpp')
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
'''
    early_profile = '''
# Must precede add_subdirectory: child directories snapshot these definitions.
option(WOWDECOMP_ISOLATED_SERVER_BUILD "Explicit isolated test server profile" OFF)
add_compile_definitions(WOWDECOMP_ISOLATED_SERVER_BUILD=$<BOOL:${WOWDECOMP_ISOLATED_SERVER_BUILD}>)
'''
    edit('CMakeLists.txt', lambda t: replace(t, 'project(wowdecomp CXX C)', 'project(wowdecomp CXX C)\n' + early_profile) + cmake)
    audit = dict(upstream=LOCK, adaptations=changes,
                 added={name: digest(root / name) for name in ['RovNativePolicy.h', 'RovNativePolicyCheck.cpp', 'RovNativeNearCheck.cpp', 'RovNativeAngleCheck.cpp']})
    (root / 'source-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    return root


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.destination.resolve(), args.archive.resolve()))
