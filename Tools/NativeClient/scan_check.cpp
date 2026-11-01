// Regression for the actual post-agreement service failure and login boundary.
// Runs inside the existing client contract executable; no game data or network.
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include "Engine/Source/Frame/Portable/Runtime.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "WoW/Source/Glue/GlueMgr.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Glue/Portable/ScriptServices.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
}
void CheckNativeScanServices() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-native-scan-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Remove { std::filesystem::path path; ~Remove() { std::filesystem::remove_all(path); } } remove{directory};
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, std::vector<uint8_t>&) { return false; });
    FrameXML::Native::Runtime xml;
    Frame::Native::Runtime layout;
    const auto config = (directory / "Config.wtf").string();
    { std::ofstream file(config); }
    ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    struct Console { ~Console() { CVar::Destroy(); ConsoleCommandDestroy(); } } console;
    GlueNative::InitializeScriptServices(2); GlueScriptEventsRegisterFunctions();
    struct Scripts { ~Scripts() { GlueScriptEventsUnregisterFunctions(); } } scripts;
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
        assert(self==ScanListener and IsScanDLLFinished()==nil);
        if event=='SCANDLL_ERROR' then
            assert(select('#',...)==1 and string.find((...), 'unavailable'));
            scanEvents=scanEvents+1;
            ScanDLLStart('https://fixture.invalid/version',''); -- reentry cannot reschedule
            assert(scanPollRejected() and scanCloseRejected());
            ScanDLLContinueAnyway(); -- unchanged AccountLogin's automatic call
            assert(IsScanDLLFinished()==nil);
            presented=true;
            if scanHandlerFails then error('authored original handler failure') end;
        elseif event=='OPEN_STATUS_DIALOG' then
            assert(scanPollRejected() and scanCloseRejected());
            local kind,reason=...;
            assert(kind=='OKAY' and string.find(reason,'Sign%-in is unavailable'));
            assert(not string.find(reason,'fixture credential'));
            notices=notices+1;
        elseif event=='CLOSE_STATUS_DIALOG' then
            assert(select('#',...)==0); closes=closes+1;
        else error('A scan success/downloading event was fabricated') end;
    )", nullptr);
    Check(listener.m_onEvent.m_luaRef > 0, "Scan listener compilation failed");
    for (const char* event : {"SCANDLL_ERROR", "SCANDLL_DOWNLOADING", "SCANDLL_FINISHED", "OPEN_STATUS_DIALOG", "CLOSE_STATUS_DIALOG"})
        Check(listener.RegisterScriptEvent(event), "Scan event registration failed");
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
    const auto execute = [](const char* source) { FrameScript_Execute(source, "authored ScanDLL refusal lifecycle", nullptr); };
    auto errors = FrameScript::Native::ScriptErrorCount();
    execute(R"(
        scanEvents=0; notices=0; closes=0; presented=false;
        assert(select('#',IsScanDLLFinished())==1 and IsScanDLLFinished()==nil);
        assert(select('#',ScanDLLContinueAnyway())==0 and IsScanDLLFinished()==nil);
        assert(not pcall(ScanDLLStart) and not pcall(ScanDLLStart,{},''));
        assert(not pcall(ScanDLLStart,'version',{}));
        assert(not pcall(DefaultServerLogin) and not pcall(DefaultServerLogin,'',{}));
        DefaultServerLogin('fixture credential account','fixture credential password');
        assert(notices==0 and scanEvents==0 and TOSAccepted()==nil and EULAAccepted()==nil);
    )");
    GlueNative::PollScriptServices();
    execute(R"(
        assert(notices==1);
        StatusDialogClick(); StatusDialogClick(); assert(closes==1);
        -- Explicit authored agreement decisions, never original-data acceptance.
        AcceptTOS(); AcceptEULA();
        DefaultServerLogin('fixture credential account','fixture credential password');
        assert(notices==1 and IsScanDLLFinished()==nil);
    )");
    GlueNative::PollScriptServices();
    execute("assert(notices==2)");
    execute("assert(scanEvents==0); ScanDLLStart('https://fixture.invalid/version',''); assert(scanEvents==0); ScanDLLStart('another','Scan.dll'); DefaultServerLogin('',''); ScanDLLContinueAnyway()");
    bool foreign=false;
    std::thread thread([&] { try { GlueNative::PollScriptServices(); } catch (const std::logic_error&) { foreign=true; } });
    thread.join(); Check(foreign,"Foreign ScanDLL polling was admitted");
    GlueNative::PollScriptServices(); GlueNative::PollScriptServices();
    execute(R"(
        assert(scanEvents==1 and presented and notices==3 and IsScanDLLFinished()==nil);
        DefaultServerLogin('fixture credential account','fixture credential password');
        ScanDLLContinueAnyway(); assert(notices==3 and IsScanDLLFinished()==nil);
    )");
    GlueNative::PollScriptServices();
    execute(R"(
        assert(notices==4 and IsScanDLLFinished()==nil);
        StatusDialogClick(); assert(closes==2);
        retainedScanStart=ScanDLLStart; retainedScanContinue=ScanDLLContinueAnyway;
        retainedScanQuery=IsScanDLLFinished; retainedLogin=DefaultServerLogin; retainedStatus=StatusDialogClick;
    )");
    Check(!CGlueMgr::m_authenticated && CGlueMgr::m_acceptedTOS && CGlueMgr::m_acceptedEULA,
        "UI failure continuation authenticated or changed agreement decisions");
    Check(FrameScript::Native::ScriptErrorCount()==errors,"ScanDLL refusal/presentation order failed");
    // A later original agreement/UI activation must receive its failure again.
    execute("presented=false; ScanDLLStart('version','')");
    GlueNative::PollScriptServices(); GlueNative::PollScriptServices();
    execute("assert(scanEvents==2 and presented and IsScanDLLFinished()==nil)");
    execute("ScanDLLStart('version',''); scanHandlerFails=true; priorNotices=notices");
    GlueNative::PollScriptServices();
    Check(FrameScript::Native::ScriptErrorCount()==errors+1 &&
        FrameScript::Native::LastScriptError().find("authored original handler failure")!=std::string::npos,
        "An original event failure was hidden or replaced");
    errors=FrameScript::Native::ScriptErrorCount();
    execute("assert(notices==priorNotices and scanEvents==3); scanHandlerFails=false");
    // Closing with a pending failure cancels both failure and dialog delivery.
    execute("ScanDLLStart('version','Scan.dll')");
    GlueScriptEventsUnregisterFunctions();
    bool closed=false;
    try { GlueNative::PollScriptServices(); } catch (const std::logic_error&) { closed=true; }
    Check(closed,"ScanDLL polling outlived Glue services");
    execute(R"(
        assert(ScanDLLStart==nil and DefaultServerLogin==nil and StatusDialogClick==nil);
        assert(not pcall(retainedScanStart,'version','') and not pcall(retainedScanContinue) and not pcall(retainedScanQuery));
        assert(not pcall(retainedLogin,'','') and not pcall(retainedStatus));
    )");
    GlueNative::InitializeScriptServices(2); GlueScriptEventsRegisterFunctions();
    GlueNative::PollScriptServices();
    execute("assert(scanEvents==3 and IsScanDLLFinished()==nil); StatusDialogClick(); assert(closes==2)");
    Check(FrameScript::Native::ScriptErrorCount()==errors && !CGlueMgr::m_authenticated,
        "ScanDLL recreation retained a failure callback or login permission");
    FrameScript_UnregisterFunction("scanPollRejected"); FrameScript_UnregisterFunction("scanCloseRejected");
    std::cout << "native-scandll-failed-ui-presented-login-refused-order-owner-close-recovery=passed\n";
}
