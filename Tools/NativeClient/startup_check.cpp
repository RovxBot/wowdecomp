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
#include <stdexcept>
#include <vector>
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
}
int main() {
    try {
        StormRtlInitialize();
        Events(); Settings(); Ordered();
        StormRtlDestroy();
        std::cout << "native-startup-contracts=passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
