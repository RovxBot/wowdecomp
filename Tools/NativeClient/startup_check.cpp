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
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/GlueMgr.h"
#include "WoW/Source/Glue/Portable/ScriptServices.h"
#include "GameVersion.h"
#include <stdexcept>
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
void GlueServices() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("wow-native-glue-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Remove { std::filesystem::path path; ~Remove() { std::filesystem::remove_all(path); } } remove{directory};
    FrameScript::Native::Runtime vm([](FrameScript::Native::SourceKind, const std::string&, std::vector<uint8_t>&) { return false; });
    FrameXML::Native::Runtime xml;
    const auto config = (directory / "Config.wtf").string();
    { std::ofstream file(config); file << "SET accountName \"saved fixture\"\n"; }
    ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    struct Console { ~Console() { CVar::Destroy(); ConsoleCommandDestroy(); } } console;
    GlueNative::RegisterConsoleVariables();
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
    EventInitialize(1, 0);
    FrameScript_Execute("QuitGame()", "native Glue close fixture", nullptr);
    Check(EventNative::Closing(), "Glue QuitGame did not close the native Event owner");
    EventDestroy();
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
        Events(); Settings(); Ordered(); GlueServices(); HtmlFiles(); CheckNativeModelIO(); CheckNativeModels();
        StormRtlDestroy();
        std::cout << "native-startup-contracts=passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
