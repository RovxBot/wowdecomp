#include "Engine/Source/Event/Portable/Loop.h"
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <fstream>
#include <chrono>
#include <clocale>
#include "Engine/Source/Gx/Portable/GlContext.h"
#include "StartupLocale.h"
#include "HtmlSource.h"
#include "LoopScriptErrors.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/GlueMgr.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "Engine/Source/Services/Portable/LegacyFiles.h"
#include "WoW/Source/Glue/Portable/ScriptServices.h"
#include "GameVersion.h"
#include "Engine/Source/Frame/Portable/Runtime.h"
#include <stdexcept>
#include <thread>
#include <vector>
void CheckNativeModelIO();
void CheckNativeModels();
namespace {
void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
std::vector<int> order;
int Low(const void*, void*) { order.push_back(1); return 1; }
int High(const void*, void*) {
    order.push_back(2); EventUnregister(EVENT_ID_IDLE, Low); return 1;
}
int Throws(const void*, void*) { throw std::runtime_error("expected callback failure"); }
void Events() {
    EventInitialize(1, 0);
    EventRegisterEx(EVENT_ID_IDLE, Low, nullptr, 0);
    EventRegisterEx(EVENT_ID_IDLE, High, nullptr, 1);
    EventNative::Dispatch(EVENT_ID_IDLE, nullptr);
    Check(order == std::vector<int>{2}, "Event priority/unregister order changed");
    EventRegister(EVENT_ID_IDLE, Throws);
    bool rejected = false;
    try { EventNative::Dispatch(EVENT_ID_IDLE, nullptr); } catch (const std::runtime_error&) { rejected = true; }
    Check(rejected, "Callback failure was hidden");
    EventUnregister(EVENT_ID_IDLE, Throws);
    EventNative::Dispatch(EVENT_ID_IDLE, nullptr); // marker must have unwound
    EventDestroy();
}
void CallbackFailuresBeforePaint() {
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, std::vector<uint8_t>&) { return false; });
    EventInitialize(1, 0);
    struct EventsCleanup { ~EventsCleanup() { EventDestroy(); } } cleanup;
    struct State { uint64_t errors; bool painted = false, fail = false; } state{FrameScript::Native::ScriptErrorCount()};
    const auto update = +[](const void*, void* raw) {
        if (static_cast<State*>(raw)->fail)
            FrameScript_Execute("error('authored update failure')", "native loop fixture", nullptr);
        return 1;
    };
    const auto paint = +[](const void*, void* raw) {
        auto& state = *static_cast<State*>(raw);
        CheckLoopScriptErrors(state.errors);
        state.painted = true;
        return 1;
    };
    EventRegisterEx(EVENT_ID_UPDATE, update, &state, EVENT_PRIORITY_NORMAL);
    EventRegisterEx(EVENT_ID_PAINT, paint, &state, EVENT_PRIORITY_NORMAL);
    EventNative::Tick(0, 0);
    Check(state.painted, "Successful callbacks did not reach paint");
    state.painted = false; state.fail = true;
    bool rejected = false;
    try { EventNative::Tick(0, 1); }
    catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("authored update failure") != std::string::npos;
    }
    Check(rejected && !state.painted && FrameScript::Native::ScriptErrorCount() == state.errors + 1,
        "Protected Lua failure reached paint or lost its original diagnostic");
    // The caller may retire the failed session. Recover the authored fixture to
    // verify Event markers and the original protected handler also unwound.
    state.errors = FrameScript::Native::ScriptErrorCount(); state.fail = false;
    EventNative::Tick(0, 2);
    Check(state.painted, "Failed pre-paint check stranded Event dispatch");
    std::cout << "native-loop-protected-error-before-paint=passed\n";
}
void Settings() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-native-cvar-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    const auto config = (directory / "Config.wtf").string();
    ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    auto* value = CVar::Register("nativeStartupValue", "fixture", CVAR_ARCHIVE, "41", nullptr, 0, true, nullptr, false);
    Check(value && value->m_intValue == 41, "CVar default changed");
    value->Set("43", true, false, false, true);
    Check(CVar::SaveAll() == 1 && std::filesystem::is_regular_file(config), "CVar did not save to explicit settings file");
    CVar::Destroy(); ConsoleCommandDestroy();
    ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    value = CVar::Register("nativeStartupValue", "fixture", CVAR_ARCHIVE, "41", nullptr, 0, true, nullptr, false);
    Check(value && value->m_intValue == 43, "Original SET settings did not round-trip");
    CVar::Destroy(); ConsoleCommandDestroy();
    std::filesystem::remove_all(directory);
}
void Ordered() {
    const std::map<std::string, std::string> files{
        {"Interface\\Test\\Test.toc", "# fixture\nA.lua\nRoot.xml\n"},
        {"Interface\\Test\\A.lua", "order = 'a'"},
        {"Interface\\Test\\Root.xml", "<Ui><Include file=\"Child.xml\"/><Script file=\"B.lua\">order=order..'d'</Script></Ui>"},
        {"Interface\\Test\\Child.xml", "<Ui><Script>order=order..'b'</Script></Ui>"},
        {"Interface\\Test\\B.lua", "order=order..'c'"},
        {"Interface\\Test\\Cycle.xml", "<Ui><Include file=\"Cycle.xml\"/></Ui>"}
    };
    FrameScript::Native::Runtime vm([&](FrameScript::Native::SourceKind, const std::string& path, std::vector<uint8_t>& bytes) {
        auto it = files.find(path); if (it == files.end()) return false;
        bytes.assign(it->second.begin(), it->second.end()); return true;
    });
    FrameXML::Native::Runtime xml;
    CharSelectRegisterScriptFunctions();
    auto characterCount = [] {
        lua_getglobal(s_context, "GetNumCharacters"); lua_call(s_context, 0, 1);
        const auto count = lua_tonumber(s_context, -1); lua_pop(s_context, 1); return count;
    };
    Check(characterCount() == 0, "Original pre-authentication character list was not empty");
    CCharacterSelection::s_characterList.SetCount(1);
    Check(characterCount() == 1, "Character query did not read the original list state");
    CCharacterSelection::s_characterList.Clear();
    Check(characterCount() == 0, "Character list reset failed");
    CharSelectUnregisterScriptFunctions();
    MD5_CTX actual, expected; MD5Init(&actual); MD5Init(&expected); CStatus status;
    Check(FrameXML_CreateFrames("Interface\\Test\\Test.toc", nullptr, &actual, &status), "Ordered creation failed");
    for (const char* file : {"Test.toc", "A.lua", "Root.xml", "Child.xml", "B.lua"}) {
        const auto& bytes = files.at(std::string("Interface\\Test\\") + file);
        MD5Update(&expected, reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    }
    uint8_t a[16], b[16]; MD5Final(a, &actual); MD5Final(b, &expected);
    Check(!std::memcmp(a, b, 16), "Post-creation byte order mismatch");
    lua_getglobal(s_context, "order"); Check(!std::strcmp(lua_tostring(s_context, -1), "abcd"), "Lua Include/Script order changed"); lua_pop(s_context, 1);
    bool cycle = false;
    try { FrameXML_ProcessFile("Interface\\Test\\Cycle.xml", nullptr, nullptr, &status); } catch (const std::invalid_argument&) { cycle = true; }
    Check(cycle, "Include cycle accepted");
    Check(FrameXML_ProcessFile("Interface\\Test\\Root.xml", nullptr, nullptr, &status), "Failed traversal did not release trees/recursion state");
}
void ScanServices() {
    GlueScriptEventsInitialize();
    FrameScript_CreateEvents(g_glueScriptEvents, NUM_GLUE_SCRIPT_EVENTS);
    const int meta = FrameScript_Object::CreateScriptMetaTable(s_context, +[](lua_State*) {});
    struct Meta { int ref; ~Meta() { luaL_unref(s_context, LUA_REGISTRYINDEX, ref); } } release{meta};
    struct Listener final : FrameScript_Object {
        int meta;
        explicit Listener(int value) : meta(value) { RegisterScriptObject("ScanListener"); }
        const char* GetName() override { return "ScanListener"; }
        int32_t GetScriptMetaTable() override { return meta; }
        bool IsA(int32_t) override { return false; }
    } listener(meta);
    listener.m_onEvent.m_luaRef = FrameScript_CompileFunction("*OnEvent", "return function(self,event,...) %s end", R"(
        assert(self==ScanListener and event=='SCANDLL_ERROR');
        assert(select('#',...)==1 and string.find((...), 'unavailable'));
        scanEvents=scanEvents+1;
        assert(IsScanDLLFinished()==nil);
        ScanDLLStart('https://fixture.invalid/version',''); -- cannot schedule another failure
        assert(scanPollRejected() and scanCloseRejected());
        if scanUnhandled then ScanDLLContinueAnyway() end;
        local ok,reason=pcall(ScanDLLContinueAnyway);
        assert(not ok and string.find(reason,'scan approval was not granted'));
        assert(IsScanDLLFinished()==nil);
    )", nullptr);
    Check(listener.m_onEvent.m_luaRef > 0 && listener.RegisterScriptEvent("SCANDLL_ERROR") &&
        listener.RegisterScriptEvent("SCANDLL_DOWNLOADING") && listener.RegisterScriptEvent("SCANDLL_FINISHED"),
        "Scan event listener creation failed");
    FrameScript_RegisterFunction("scanPollRejected", +[](lua_State* L) -> int {
        bool rejected=false;
        try { GlueNative::PollScriptServices(); } catch (const std::logic_error&) { rejected=true; }
        lua_pushboolean(L,rejected); return 1;
    });
    FrameScript_RegisterFunction("scanCloseRejected", +[](lua_State* L) -> int {
        bool rejected=false;
        try { GlueScriptEventsUnregisterFunctions(); } catch (const std::logic_error&) { rejected=true; }
        lua_pushboolean(L,rejected); return 1;
    });
    const auto execute = [](const char* source) { FrameScript_Execute(source, "authored ScanDLL failure lifecycle", nullptr); };
    auto errors = FrameScript::Native::ScriptErrorCount();
    execute(R"(
        scanEvents=0; scanUnhandled=false;
        assert(select('#',IsScanDLLFinished())==1 and IsScanDLLFinished()==nil);
        assert(select('#',ScanDLLContinueAnyway())==0 and IsScanDLLFinished()==nil);
        assert(not pcall(ScanDLLStart) and not pcall(ScanDLLStart,{},''));
        assert(not pcall(ScanDLLStart,'version',{}));
    )");
    GlueNative::PollScriptServices(); // invalid starts have no pending event
    execute("assert(scanEvents==0); ScanDLLStart('https://fixture.invalid/version',''); assert(scanEvents==0); ScanDLLStart('another','Scan.dll'); assert(IsScanDLLFinished()==nil); ScanDLLContinueAnyway()");
    bool foreign=false;
    std::thread thread([&] { try { GlueNative::PollScriptServices(); } catch (const std::logic_error&) { foreign=true; } });
    thread.join(); Check(foreign,"Foreign ScanDLL polling was admitted");
    GlueNative::PollScriptServices();
    GlueNative::PollScriptServices();
    execute("assert(scanEvents==1 and IsScanDLLFinished()==nil)");
    Check(FrameScript::Native::ScriptErrorCount()==errors,"ScanDLL error order/result/reentry contracts failed");
    execute("retainedScanStart=ScanDLLStart; retainedScanContinue=ScanDLLContinueAnyway; retainedScanQuery=IsScanDLLFinished");
    GlueScriptEventsUnregisterFunctions();
    bool closed=false;
    try { GlueNative::PollScriptServices(); } catch (const std::logic_error&) { closed=true; }
    Check(closed,"ScanDLL polling outlived Glue services");
    execute(R"(
        assert(ScanDLLStart==nil and ScanDLLContinueAnyway==nil and IsScanDLLFinished==nil);
        assert(not pcall(retainedScanStart,'version','') and not pcall(retainedScanContinue) and not pcall(retainedScanQuery));
    )");
    GlueNative::InitializeScriptServices(2); GlueScriptEventsRegisterFunctions();
    GlueNative::PollScriptServices();
    execute("assert(scanEvents==1 and IsScanDLLFinished()==nil); ScanDLLStart('version',''); scanUnhandled=true");
    GlueNative::PollScriptServices(); // original AccountLogin's automatic Continue must fail closed
    Check(FrameScript::Native::ScriptErrorCount()==errors+1 &&
        FrameScript::Native::LastScriptError().find("scan approval was not granted")!=std::string::npos,
        "Automatic ScanDLL continuation granted approval or lost its failure");
    errors=FrameScript::Native::ScriptErrorCount();
    GlueNative::PollScriptServices();
    execute("assert(scanEvents==2 and IsScanDLLFinished()==nil); scanUnhandled=false");
    // Closing with a pending failure cancels delivery; recreation starts clean.
    GlueScriptEventsUnregisterFunctions();
    GlueNative::InitializeScriptServices(2); GlueScriptEventsRegisterFunctions();
    execute("ScanDLLStart('version','Scan.dll')");
    GlueScriptEventsUnregisterFunctions();
    GlueNative::InitializeScriptServices(2); GlueScriptEventsRegisterFunctions();
    GlueNative::PollScriptServices();
    execute("assert(scanEvents==2 and IsScanDLLFinished()==nil)");
    Check(FrameScript::Native::ScriptErrorCount()==errors,"ScanDLL cancellation/recreation retained a failure callback");
    FrameScript_UnregisterFunction("scanPollRejected"); FrameScript_UnregisterFunction("scanCloseRejected");
    std::cout << "native-scandll-unavailable-deferred-error-no-approval-order-owner-close-recovery=passed\n";
}
void GlueServices() {
    Services::Native::FileQueue queue;
    Services::Native::LegacyFiles files(queue, [](const std::string&, const Services::Native::Cancellation&) -> Services::Native::ReadResult {
        throw std::runtime_error("Glue activation fixture has no model data");
    });
    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-native-glue-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Remove { std::filesystem::path path; ~Remove() { std::filesystem::remove_all(path); } } remove{directory};
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, std::vector<uint8_t>&) { return false; });
    FrameXML::Native::Runtime xml;
    Frame::Native::Runtime layout; // Original StringToBOOL shares the font/XML owner.
    const auto config = (directory / "Config.wtf").string();
    { std::ofstream file(config); file << "SET accountName \"saved fixture\"\nSET readTOS \"1\"\nSET readEULA \"-2\"\nSET readTerminationWithoutNotice \"-2\"\nSET readScanning \"-1\"\n"; }
    ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    struct Console { ~Console() { CVar::Destroy(); ConsoleCommandDestroy(); } } console;
    GlueNative::InitializeScriptServices(2);
    GlueScriptEventsRegisterFunctions();
    struct Scripts { ~Scripts() { GlueScriptEventsUnregisterFunctions(); } } scripts;
    auto* option = CVar::Register("glueFixture", "fixture", 0, "7", nullptr, 0, false, nullptr, false);
    auto* locked = CVar::Register("glueReadOnly", "fixture", 4, "1", nullptr, 0, false, nullptr, false);
    Check(option && locked, "Glue CVar fixtures failed");
    const auto errors = FrameScript::Native::ScriptErrorCount();
    FrameScript_Execute(R"lua(
        VERSION = 'localized version'; RELEASE_BUILD = 'localized release';
        assert(select('#', GetBuildInfo()) == 5);
        local label, release, version, build, date = GetBuildInfo();
        assert(label == VERSION and release == RELEASE_BUILD);
        assert(version == ')lua" GAME_VERSION R"lua(' and build == ')lua" GAME_BUILD_STRING R"lua(' and date == ')lua" GAME_BUILD_DATE R"lua(');
        VERSION = 'changed label'; assert(GetBuildInfo() == VERSION);
        assert(GetLocale() == 'enUS' and IsLinuxClient() == 1);
        assert(IsSystemSupported() == 1);
        assert(IsTrialAccount()==nil);
        assert(select('#',GetServerName())==4);
        local realm,pvp,rp,down=GetServerName();
        assert(realm=='' and pvp==nil and rp==nil and down==1);
        SetCVar('realmName','authored saved realm');
        assert(GetServerName()=='authored saved realm');
        SetCVar('realmName','');
        assert(TOSAccepted()==1 and ShowTOSNotice()==nil);
        assert(EULAAccepted()==nil and ShowEULANotice()==1);
        assert(TerminationWithoutNoticeAccepted()==nil and ShowTerminationWithoutNoticeNotice()==1);
        assert(ScanningAccepted()==nil and ShowScanningNotice()==1);
        assert(ContestAccepted()==nil and ShowContestNotice()==nil);
        -- This VM has no agreement files or game data: exercise authored state only.
        assert(select('#',AcceptEULA())==0 and EULAAccepted()==1 and ShowEULANotice()==nil);
        assert(GetCVar('readEULA')=='-1');
        AcceptTerminationWithoutNotice(); assert(GetCVar('readTerminationWithoutNotice')=='-1');
        AcceptScanning(); AcceptContest();
        assert(ScanningAccepted()==1 and ShowScanningNotice()==nil and GetCVar('readScanning')=='1');
        assert(ContestAccepted()==1 and GetCVar('readContest')=='1');
        assert(ShowChangedOptionWarnings() == nil and select('#',GetChangedOptionWarnings())==0);
        assert(select('#',SetCurrentScreen('login'))==0);
        assert(not pcall(SetCurrentScreen,{}) and not pcall(SetCurrentScreen,'charselect'));
        assert(GetClientExpansionLevel() == 3);
        assert(IsWindowsClient() == nil and IsMacClient() == nil);
        assert(GetSavedAccountName() == 'saved fixture');
        SetSavedAccountName('changed fixture'); assert(GetSavedAccountName() == 'changed fixture');
        assert(GetSavedAccountList() == ''); SetSavedAccountList('one|two'); assert(GetSavedAccountList() == 'one|two');
        assert(GetUsesToken() == false); SetUsesToken(true); assert(GetUsesToken() == true);
        assert(not pcall(SetUsesToken, 1));
        assert(not pcall(SetSavedAccountName, {}));
        assert(GetCVar('glueFixture') == '7' and GetCVarDefault('glueFixture') == '7');
        SetCVar('glueFixture', 9); assert(GetCVar('glueFixture') == '9' and GetCVarBool('glueFixture') == 1);
        SetCVar('glueFixture'); assert(GetCVar('glueFixture') == '0' and GetCVarBool('glueFixture') == nil);
        assert(GetCVarBool('missingGlueCVar') == nil);
        assert(not pcall(GetCVar, 'missingGlueCVar'));
        assert(not pcall(GetCVarDefault, 'missingGlueCVar'));
        assert(not pcall(SetCVar, 'missingGlueCVar', '1'));
        assert(not pcall(SetCVar, 'glueReadOnly', '0') and GetCVar('glueReadOnly') == '1');
        RunScript('glueNestedScript = 42'); assert(glueNestedScript == 42);
    )lua", "native Glue services fixture", nullptr);
    Check(FrameScript::Native::ScriptErrorCount() == errors, "Original Glue build/CVar contracts failed");
    Check(option->m_intValue == 0 && locked->m_intValue == 1, "Glue scripts bypassed original CVar state");
    CGlueMgr::m_authenticated=1;
    FrameScript_Execute("assert(not pcall(GetServerName) and not pcall(IsTrialAccount))", "unavailable authenticated realm metadata", nullptr);
    CGlueMgr::m_authenticated=0;
    auto* converted=CVar::Register("converted","authored trial preference",0,"0",nullptr,0,false,nullptr,false);
    FrameScript_Execute("assert(IsTrialAccount()==1)", "original no-login trial preference", nullptr);
    converted->Set("1",true,false,false,true);
    FrameScript_Execute("assert(IsTrialAccount()==nil)", "original no-login converted preference", nullptr);
    Check(!std::strcmp(CGlueMgr::m_currentScreen,"login"), "Rejected screen transition mutated lifecycle state");
    FrameScript_Execute("NATIVE_WARNING='authored warning'", "option warning fixture", nullptr);
    CGlueMgr::AddChangedOptionWarning("NATIVE_WARNING");
    FrameScript_Execute(R"(
        NATIVE_WARNING=nil; collectgarbage('collect');
        assert(ShowChangedOptionWarnings()==1 and select('#',GetChangedOptionWarnings())==1);
        assert(GetChangedOptionWarnings()=='authored warning');
        assert(select('#',AcceptChangedOptionWarnings())==0);
        assert(ShowChangedOptionWarnings()==nil and select('#',GetChangedOptionWarnings())==0);
    )", "owned option warning fixture", nullptr);
    Check(FrameScript::Native::ScriptErrorCount() == errors, "Original Glue warning ownership/acknowledgement failed");
    ScanServices();
    const auto scanErrors = FrameScript::Native::ScriptErrorCount();
    EventInitialize(1, 0);
    FrameScript_Execute("QuitGame()", "native Glue close fixture", nullptr);
    Check(EventNative::Closing(), "Glue QuitGame did not close the native Event owner");
    EventDestroy();
    FrameScript_Execute("retainedGlueQuery = GetSavedAccountName; retainedScreenSetter=SetCurrentScreen; retainedAgreement=AcceptEULA; retainedAgreementQuery=EULAAccepted", "retain Glue closure", nullptr);
    GlueScriptEventsUnregisterFunctions();
    FrameScript_Execute("assert(GetSavedAccountName == nil and not pcall(retainedGlueQuery) and not pcall(retainedScreenSetter,'login')); assert(EULAAccepted==nil and not pcall(retainedAgreement) and not pcall(retainedAgreementQuery))", "closed Glue closure", nullptr);
    Check(!*CGlueMgr::m_currentScreen && CGlueMgr::m_numChangedOptionWarnings==0, "Glue activation state survived close");
    Check(FrameScript::Native::ScriptErrorCount() == scanErrors, "Retained Glue closure used a closed CVar owner");
    for (uint8_t expansion : {0, 1, 2}) {
        GlueNative::InitializeScriptServices(expansion);
        GlueScriptEventsRegisterFunctions();
        if (expansion==0) {
            FrameScript_Execute(R"(
                assert(EULAAccepted()==nil and ShowEULANotice()==1);
                assert(TerminationWithoutNoticeAccepted()==nil and ShowTerminationWithoutNoticeNotice()==1);
                AcceptEULA(); AcceptTerminationWithoutNotice();
                assert(GetCVar('readEULA')=='1' and GetCVar('readTerminationWithoutNotice')=='1');
            )", "authored agreement reread state", nullptr);
        } else {
            FrameScript_Execute("assert(EULAAccepted()==1 and ShowEULANotice()==nil and TerminationWithoutNoticeAccepted()==1)",
                "authored durable agreement state", nullptr);
        }
        lua_getglobal(s_context, "GetClientExpansionLevel"); lua_call(s_context, 0, 1);
        Check(lua_tonumber(s_context, -1) == expansion + 1, "Installed expansion query changed"); lua_pop(s_context, 1);
        GlueScriptEventsUnregisterFunctions();
    }
    Check(FrameScript::Native::ScriptErrorCount()==scanErrors, "Original agreement reread/durable state changed");
}
void HtmlFiles() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-native-html-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Remove { std::filesystem::path path; ~Remove() { std::filesystem::remove_all(path); } } remove{directory};
    std::filesystem::create_directories(directory / "data/enUS");
    const std::string contents = "<HTML><BODY><P>Original HTML reader fixture</P></BODY></HTML>";
    { std::ofstream file(directory / "data/enUS/tos.html"); file << contents; }
    { std::ofstream file(directory / "outside.html"); file << "outside"; }
    std::filesystem::create_symlink(directory / "outside.html", directory / "data/enUS/escape.html");
    { std::ofstream file(directory / "data/enUS/large.html"); file.seekp(8 * 1024 * 1024); file.put('x'); }
    Blizzard::File::Native::Paths paths;
    Check(Blizzard::File::Native::ConfigurePaths((directory / "data").c_str(), (directory / "settings").c_str(),
        (directory / "cache").c_str(), paths), "HTML fixture roots failed");
    Services::Native::FileQueue queue;
    FrameScript::Native::Runtime vm([&](FrameScript::Native::SourceKind kind, const std::string& path, std::vector<uint8_t>& bytes) {
        return kind == FrameScript::Native::SourceKind::Html && GlueNative::ReadLocaleHtml(paths, queue, path, bytes);
    });
    std::string normalized; std::vector<uint8_t> bytes;
    Check(FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Html, "TOS.HTML", normalized, bytes) &&
        std::string(bytes.begin(), bytes.end()) == contents, "Original locale HTML bytes were not retained");
    Check(!FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Interface, "tos.html", normalized, bytes),
        "Host HTML was supplied as interface source");
    for (const char* name : {"missing.html", "escape.html", "large.html", "../outside.html", "sub/tos.html", "tos.lua"})
        Check(!FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Html, name, normalized, bytes), "Invalid HTML path admitted");
    { std::ofstream file(directory / "data/enUS/TOS.HTML"); file << contents; }
    Check(!FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Html, "tos.html", normalized, bytes), "Ambiguous locale HTML admitted");
}
}
int main() {
    try {
        StormRtlInitialize();
        InitializeNativeCharacterLocale();
        { Gx::Native::GlContext context("Waystone — original 12340 startup", 320, 240, true); }
        Events(); CallbackFailuresBeforePaint(); Settings(); Ordered(); GlueServices(); HtmlFiles(); CheckNativeModelIO(); CheckNativeModels();
        StormRtlDestroy();
        std::cout << "native-startup-contracts=passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
