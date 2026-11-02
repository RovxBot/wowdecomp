// Regression for the actual post-agreement service failure and login boundary.
// Runs inside the existing client contract executable; no game data or network.
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include "Engine/Source/Frame/Portable/Runtime.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
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
#include <cstring>
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
#include "Engine/Source/Event/Portable/Loop.h"
#include "Tools/NativeNet/test_server_peer.h"
#include "Engine/Source/Services/Portable/LegacyFiles.h"
#include "Engine/Source/Base/CDataStore.h"
#include "WoW/Source/Glue/Portable/RealmData.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include <unordered_map>
#endif

namespace {
void Check(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
}
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
void CheckNativeTestLoginUI() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-test-login-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Remove { std::filesystem::path path; ~Remove() { std::filesystem::remove_all(path); } } remove{directory};
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, std::vector<uint8_t>&) { return false; });
    FrameXML::Native::Runtime xml;
    Frame::Native::Runtime layout;
    const auto config = (directory / "Config.wtf").string();
    { std::ofstream file(config); }
    ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    struct Console { ~Console() { CVar::Destroy(); ConsoleCommandDestroy(); } } console;
    EventInitialize(1,0);
    struct Events { ~Events() { EventDestroy(); } } events;
    Services::Native::FileQueue files;
    // Single authored records exercise the original DBC reader/component owner
    // for the empty-screen transition. They are not original data or characters.
    std::unordered_map<std::string,uint32_t> characterFields;
#define CHARACTER_TABLE(T) characterFields.emplace(Blizzard::Mopaq::Native::Name(T::GetFilename()),uint32_t(T::s_numColumns))
    CHARACTER_TABLE(AreaTableRec); CHARACTER_TABLE(CharHairGeosetsRec); CHARACTER_TABLE(CharSectionsRec);
    CHARACTER_TABLE(CharacterFacialHairStylesRec); CHARACTER_TABLE(ChrClassesRec); CHARACTER_TABLE(ChrRacesRec);
    CHARACTER_TABLE(CreatureDisplayInfoRec); CHARACTER_TABLE(CreatureFamilyRec); CHARACTER_TABLE(CreatureModelDataRec);
    CHARACTER_TABLE(HelmetGeosetVisDataRec); CHARACTER_TABLE(ItemDisplayInfoRec); CHARACTER_TABLE(ItemVisualEffectsRec);
    CHARACTER_TABLE(ItemVisualsRec); CHARACTER_TABLE(LightParamsRec); CHARACTER_TABLE(ParticleColorRec);
    CHARACTER_TABLE(SpellVisualEffectNameRec); CHARACTER_TABLE(SpellVisualKitRec);
#undef CHARACTER_TABLE
    Services::Native::LegacyFiles legacy(files,[&](const std::string& path, const Services::Native::Cancellation&) {
        Services::Native::ReadResult result;
        const bool categories=path==Blizzard::Mopaq::Native::Name("DBFilesClient\\Cfg_Categories.dbc");
        const bool configs=path==Blizzard::Mopaq::Native::Name("DBFilesClient\\Cfg_Configs.dbc");
        if (path=="ROW.BLP" || path=="PENDING-ROW.BLP") {
            result.file.bytes.resize(152);
            auto& bytes=result.file.bytes;
            const auto put=[&](unsigned at,uint32_t value) {
                for (unsigned i=0;i<4;++i) bytes[at+i]=uint8_t(value>>(i*8));
            };
            put(0,0x32504c42); put(4,1); bytes[8]=3; bytes[9]=8; bytes[11]=1;
            put(12,1); put(16,1); put(20,148); put(84,4);
            return result;
        }
        const auto table=characterFields.find(path);
        if (table!=characterFields.end()) {
            CDataStore dbc;
            const uint32_t fields=table->second;
            dbc.Put(uint32_t(0x43424457)).Put(uint32_t(1)).Put(fields).Put(fields*4).Put(uint32_t(1));
            for (uint32_t i=0;i<fields;++i)
                dbc.Put(uint32_t(i==0 || (i==1 && path==Blizzard::Mopaq::Native::Name(CharSectionsRec::GetFilename()))));
            dbc.Put(uint8_t(0));
            result.file.bytes.assign(dbc.m_data,dbc.m_data+dbc.m_size);
            return result;
        }
        if (!categories && !configs) { result.status=Services::Native::ReadStatus::Failed; return result; }
        CDataStore dbc;
        const char names[]="\0Authored category\0";
        const uint32_t fields=categories ? 21 : 4;
        dbc.Put(uint32_t(0x43424457)).Put(uint32_t(1)).Put(fields).Put(fields*4);
        dbc.Put(uint32_t(categories ? sizeof(names) : 1));
        for (uint32_t i=0;i<fields;++i)
            dbc.Put(uint32_t(categories ? (i<3 || i==4 ? 1 : 0) : (i==1 || i==2 ? 1 : 0)));
        dbc.PutData(names,categories ? sizeof(names) : 1);
        result.file.bytes.assign(dbc.m_data,dbc.m_data+dbc.m_size);
        return result;
    });
    const auto execute = [](const char* code) { FrameScript_Execute(code,"authored test-server UI",nullptr); };
    auto* endpoint = CVar::Register("realmlist", "Authored endpoint", 0, "", nullptr, CATEGORY_NET, false, nullptr, false);
    bool rejected=false;
    try { GlueNative::InitializeScriptServices(2,true); } catch (const std::invalid_argument&) { rejected=true; }
    Check(rejected,"Test profile accepted missing external endpoint");
    bool scriptsOpen=false;
    struct Scripts { bool& open; ~Scripts() { if (open) GlueScriptEventsUnregisterFunctions(); } } scripts{scriptsOpen};
    GlueScriptEventsInitialize(); FrameScript_CreateEvents(g_glueScriptEvents,NUM_GLUE_SCRIPT_EVENTS);
    CharSelectRegisterScriptFunctions();
    struct CharacterScripts { ~CharacterScripts() { CharSelectUnregisterScriptFunctions(); } } characterScripts;
    const int meta=FrameScript_Object::CreateScriptMetaTable(s_context,+[](lua_State*) {});
    struct Meta { int ref; ~Meta() { luaL_unref(s_context,LUA_REGISTRYINDEX,ref); } } release{meta};
    struct Listener final : FrameScript_Object {
        int meta;
        explicit Listener(int value) : meta(value) { RegisterScriptObject("TestLoginListener"); }
        const char* GetName() override { return "TestLoginListener"; }
        int32_t GetScriptMetaTable() override { return meta; }
        bool IsA(int32_t) override { return false; }
    } listener(meta);
    listener.m_onEvent.m_luaRef=FrameScript_CompileFunction("*OnEvent","return function(self,event,...) %s end",R"(
        assert(IsScanDLLFinished()==nil);
        if event=='SCANDLL_ERROR' then ScanDLLContinueAnyway();
        elseif event=='SCANDLL_FINISHED' then scanSuccess=scanSuccess+1;
        elseif event=='OPEN_STATUS_DIALOG' then
            local kind,reason=...;
            assert(not string.find(reason,'authored%-password') and not string.find(reason,'fixture'));
            if kind=='OKAY' and (string.find(reason,'advertised realm') or string.find(reason,'sign%-in failed')
                or string.find(reason,'realm list failed') or string.find(reason,'unsupported')
                or string.find(reason,'world connection failed')) then
                testTerminal=reason; testDone=true;
            end;
        elseif event=='GET_PREFERRED_REALM_INFO' or event=='OPEN_REALM_LIST' then
            assert(GetNumRealms()==2);
            local name,count,locked,down=GetRealmInfo(1);
            assert(name=='Fixture One' and count==3 and not locked and not down);
            assert(not pcall(GetRealmInfo,0/0));
            assert(not pcall(ChangeRealm,1e100));
            if testWantWorld then ChangeRealm(1,1);
            else testTerminal='received 2 advertised realm'; testDone=true; end;
        elseif event=='SET_GLUE_SCREEN' then
            local screen=...; SetCurrentScreen(screen);
        elseif event=='CHARACTER_LIST_UPDATE' then
            assert(GetNumCharacters()==0 and IsConnectedToServer());
            testTerminal='empty character selection'; testDone=true;
        elseif event=='UPDATE_SELECTED_CHARACTER' then
            assert(GetNumCharacters()==0);
        elseif event=='CLIENT_ACCOUNT_MISMATCH' then
            local account,client=...; assert(account==3 and client==2);
            testTerminal='expansion mismatch'; testDone=true;
        elseif event~='CLOSE_STATUS_DIALOG' then error('Unexpected test login event') end;
    )",nullptr);
    Check(listener.m_onEvent.m_luaRef>0,"Test login listener compilation failed");
    for (const char* event : {"SCANDLL_ERROR","SCANDLL_FINISHED","OPEN_STATUS_DIALOG","CLOSE_STATUS_DIALOG",
                            "GET_PREFERRED_REALM_INFO","OPEN_REALM_LIST","SET_GLUE_SCREEN",
                            "CHARACTER_LIST_UPDATE","UPDATE_SELECTED_CHARACTER","CLIENT_ACCOUNT_MISMATCH"})
        Check(listener.RegisterScriptEvent(event),"Test login event registration failed");
    for (int scenario=0; scenario<8; ++scenario) {
        const auto errors=FrameScript::Native::ScriptErrorCount();
        CheckNativeTestServerExchange(+[] { EventNative::Tick(0,0); GlueNative::PollScriptServices(); },
        [&](const char* address) {
            endpoint->Set(address,true,false,false,true);
            GlueNative::InitializeScriptServices(2,true); GlueScriptEventsRegisterFunctions();
            scriptsOpen=true;
            execute("SetCurrentScreen('login')");
            execute(scenario>=5 ? "testWantWorld=true" : "testWantWorld=false");
            // Only authored fixtures accept agreements here; product startup never does.
            execute(R"(
                testDone=false; testTerminal=''; scanSuccess=0;
                DefaultServerLogin('fixture','authored-password');
            )");
            GlueNative::PollScriptServices();
            execute("StatusDialogClick(); AcceptTOS(); AcceptEULA(); ScanDLLStart('fixture','')");
            GlueNative::PollScriptServices();
            execute("assert(IsScanDLLFinished()==nil); StatusDialogClick()");
            execute(R"(
                assert(not pcall(DefaultServerLogin,'fixture',string.char(0)..'secret'));
                assert(not pcall(DefaultServerLogin,string.rep('x',256),'secret'));
                assert(not pcall(DefaultServerLogin,'fixture',''));
            )");
            // The actual original login wrapper, not a virtual fixture callback.
            execute("DefaultServerLogin('fixture','authored-password'); DefaultServerLogin('duplicate','duplicate'); assert(IsScanDLLFinished()==nil)");
        },[&] {
            lua_getglobal(s_context,"testDone"); const bool done=lua_toboolean(s_context,-1); lua_pop(s_context,1); return done;
        },scenario);
        lua_getglobal(s_context,"testTerminal");
        const std::string terminal=lua_tostring(s_context,-1); lua_pop(s_context,1);
        const char* expected[]={"received 2 advertised realm", "LOGIN_FAILED", "LOGIN_BADVERSION", "sign-in failed", "unsupported", "world connection failed (server/status code 13)", "empty character selection", "expansion mismatch"};
        Check(terminal.find(expected[scenario])!=std::string::npos,"UI hid or changed authentication outcome");
        if (scenario==6) {
            Check(CCharacterComponent::s_bComponentThread==0 && CCharacterComponent::s_bComponentCompression==0,
                "Character profile enabled unqualified worker/compression");
            // Resources are authored and explicitly attached by this fixture,
            // never represented as server characters or observed appearance.
            for (unsigned cycle=0;cycle<3;++cycle) {
                auto* held=TextureCacheCreateTexture("row.blp");
                TCTEXTUREINFO info{};
                Check(TextureCacheGetInfo(held,info,1),"Authored row texture did not load");
                auto* row=CCharacterSelection::s_characterList.New();
                row->m_component=CCharacterComponent::AllocComponent();
                Check(row->m_component!=nullptr,"Original component allocation failed");
                row->m_component->m_baseVariation[0].m_texture[0]=TextureCacheCopyTexture(held);
                auto* pending=TextureCacheCreateTexture("pending-row.blp");
                row->m_component->m_section[0].m_layerTex[0]=pending;
                (void)TextureCacheGetInfo(pending,info,0);
                CCharacterSelection::Shutdown();
                Check(CCharacterSelection::s_characterList.Count()==0 && held->m_refCount==1 && !CGlueMgr::m_characterInfo,
                    "Original row shutdown retained resources or selected info");
                TextureCacheDestroyTexture(held);
                files.Poll(); legacy.CheckCanClose();
            }
            execute("assert(GetAccountExpansionLevel()==2); DisconnectFromServer()");
            GlueNative::PollScriptServices();
            Check(!strcmp(CGlueMgr::m_currentScreen,"login"),"Character disconnect did not return to original login");
        }
        if (!scenario) execute("RealmListDialogCancelled()");
        execute("assert(IsScanDLLFinished()==nil and scanSuccess==0); StatusDialogClick()");
        GlueNative::PollScriptServices();
        Check(!CGlueMgr::m_authenticated,"Realm-only test profile entered character/world state");
        Check(FrameScript::Native::ScriptErrorCount()==errors,"Test-server UI Lua contract failed");
        // Cancel both a started connection and a queued credential before close.
        execute("DefaultServerLogin('fixture','authored-password')");
        GlueNative::PollScriptServices();
        execute("StatusDialogClick()"); GlueNative::PollScriptServices();
        execute("DefaultServerLogin('fixture','authored-password')");
        GlueScriptEventsUnregisterFunctions();
        scriptsOpen=false;
        EventNative::Tick(0,0);
    }
    // Recreating Glue never retains the runtime allowance.
    GlueNative::InitializeScriptServices(2); GlueScriptEventsRegisterFunctions();
    scriptsOpen=true;
    execute("DefaultServerLogin('fixture','authored-password'); assert(IsScanDLLFinished()==nil)");
    GlueNative::PollScriptServices(); GlueScriptEventsUnregisterFunctions(); scriptsOpen=false;
    std::cout << "native-test-server-original-login-ui-proof-realms-cancel-close-default-recovery=passed\n";
}
#endif
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
