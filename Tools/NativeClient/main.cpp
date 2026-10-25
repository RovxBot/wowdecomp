#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Services/Portable/Texture.h"
#include "Engine/Source/Gx/Portable/LegacyDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
#include "Engine/Source/Frame/Portable/Fonts.h"
#include "Engine/Source/Frame/Portable/Scripts.h"
#include "Engine/Source/Frame/Portable/LoginWidgets.h"
#include "Engine/Source/Frame/Portable/WindowEvents.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Event/Portable/Loop.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/Glue/Portable/Startup.h"
#include "WoW/Source/Glue/GlueMgr.h"
#include <SDL3/SDL.h>
#include <cfenv>
#include <clocale>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/resource.h>
#include "StartupLocale.h"
using Bytes = std::vector<uint8_t>;
using namespace Services::Native;
namespace {
struct Runtime {
    Runtime() {
        // Initialize character conversion for SDL/Xlib without changing the
        // original numeric locale. The C locale cannot encode our UTF-8 title.
        InitializeNativeCharacterLocale();
        // The original entry uses an 8 MiB fiber; SDL must remain on main.
        rlimit stack{};
        if (getrlimit(RLIMIT_STACK, &stack)) throw std::runtime_error("Cannot inspect main stack");
        if (stack.rlim_cur < 8 * 1024 * 1024) {
            stack.rlim_cur = 8 * 1024 * 1024;
            if (setrlimit(RLIMIT_STACK, &stack)) throw std::runtime_error("Native client needs an 8 MiB main stack");
        }
        if (std::fesetenv(FE_DFL_ENV) || std::fesetround(FE_TONEAREST)) throw std::runtime_error("Cannot initialize floating point");
        StormRtlInitialize(); OsTimeStartup(OsTimeManager::BestAvailable);
    }
    ~Runtime() { OsTimeShutdown(); StormRtlDestroy(); }
};
struct Settings {
    std::string config;
    explicit Settings(const Blizzard::File::Native::Paths& paths) {
        std::string directory;
        if (!Blizzard::File::Native::ResolveUserPath(paths, Blizzard::File::Native::UserRoot::Settings, "WTF", directory) ||
            !Blizzard::File::CreateDirectory(directory.c_str(), true) ||
            !Blizzard::File::Native::ResolveUserPath(paths, Blizzard::File::Native::UserRoot::Settings, "WTF/Config.wtf", config) ||
            config.size() >= 260)
            throw std::runtime_error("Cannot resolve original config outside data");
        ConsoleInitializeClientCommand(); CVar::Initialize(config.c_str());
    }
    ~Settings() { CVar::Destroy(); ConsoleCommandDestroy(); }
};
struct Events {
    Events() { EventInitialize(1, 0); }
    ~Events() { EventDestroy(); }
};
struct Source {
    FileQueue queue;
    std::shared_ptr<Blizzard::Mopaq::Native::Packages> packages;
    explicit Source(const Blizzard::File::Native::Paths& paths) {
        auto ticket = queue.Submit([&](const Cancellation&) {
            packages = Blizzard::Mopaq::Native::Packages::Open12340(paths, "enUS"); return ReadResult{};
        }, [](const ReadResult& result) {
            if (result.status != ReadStatus::Success) throw std::runtime_error("Original archive authentication: " + result.error);
        });
        if (!queue.Wait(ticket, std::chrono::seconds(120))) throw std::runtime_error("Archive startup timeout");
        if (!packages) throw std::runtime_error("Archive startup failed");
        std::cout << "startup-stage=archives-authenticated records=" << packages->SignatureCount() << std::endl;
    }
    Bytes Read(const std::string& path) {
        auto result = std::make_shared<ReadResult>();
        auto ticket = queue.ReadPackage(packages, path, [result](const ReadResult& value) { *result = value; });
        if (!queue.Wait(ticket, std::chrono::seconds(60)) || result->status != ReadStatus::Success)
            throw std::runtime_error("Original member read failed: " + path + ": " + result->error);
        return std::move(result->file.bytes);
    }
};
int Locale(lua_State* L) { lua_pushstring(L, "enUS"); return 1; }
int OtherPlatform(lua_State* L) { lua_pushnil(L); return 1; }
// Original GlueScriptEvents_Scripts.cpp 0x004DCC20 / 0x004DCC70.
int ScreenWidth(lua_State* L) {
    float width = NDCToDDCWidth(1.0f);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * width)); return 1;
}
int ScreenHeight(lua_State* L) {
    float height = NDCToDDCHeight(1.0f);
    lua_pushnumber(L, DDCToNDCWidth(CoordinateGetAspectCompensation() * 1024.0f * height)); return 1;
}
int Update(const void* raw, void* root) {
    static_cast<CSimpleTop*>(root)->OnLayerUpdate(static_cast<const EVENT_DATA_UPDATE*>(raw)->m_elapsedSec);
    return 1;
}
int Paint(const void*, void* root) {
    GxSceneClear(GxBufferBit_Color, CImVector(0, 0, 0, 255));
    static_cast<CSimpleTop*>(root)->OnLayerRender(); GxScenePresent(); return 1;
}
void Run(const Blizzard::File::Native::Paths& paths, uint32_t frameLimit, bool hidden) {
    Source source(paths);
    FrameScript::Native::Runtime vm([&](FrameScript::Native::SourceKind kind, const std::string& path, Bytes& bytes) {
        if (kind != FrameScript::Native::SourceKind::Interface) return false;
        try { bytes = source.Read(path); return true; } catch (...) { return false; }
    });
    FrameXML::Native::Runtime xml;
    // CVar strings close before XML's shared CStringManager and VM.
    Settings settings(paths);
    Events events;
    Gx::Native::GlContext context("Waystone — original 12340 startup", 1024, 768, hidden);
    std::cout << "gl-renderer=" << context.Info().renderer << "\ngl-version=" << context.Info().version << std::endl;
    Gx::Native::LegacyDevice device(context, [&](const std::string& path) { return source.Read(path); });
    CoordinateSetAspectRatio(1024.0f / 768.0f);
    GxRsSet(GxRs_Lighting, 0); GxRsSet(GxRs_Fog, 0); GxRsSet(GxRs_Culling, 0);
    Frame::Native::Runtime layout;
    Gxu::Native::LegacyFonts gxu([&](const std::string& path) { return source.Read(path); });
    Frame::Native::Fonts fonts;
    Frame::Native::Renderer renderer;
    TextureService images(source.queue, [&](const std::string& path, const Cancellation&) {
        ReadResult result; result.file = source.packages->Read(path); return result;
    });
    Frame::Native::Scripts scripts;
    Frame::Native::LoginWidgets widgets;
    {
        CSimpleTop root;
        FrameScript_RegisterFunction("GetLocale", Locale);
        FrameScript_RegisterFunction("IsMacClient", OtherPlatform);
        FrameScript_RegisterFunction("IsWindowsClient", OtherPlatform);
        FrameScript_RegisterFunction("GetScreenWidth", ScreenWidth);
        FrameScript_RegisterFunction("GetScreenHeight", ScreenHeight);
        GlueScriptEventsInitialize();
        FrameScript_CreateEvents(g_glueScriptEvents, NUM_GLUE_SCRIPT_EVENTS);
        std::cout << "startup-stage=original-frame-root" << std::endl;
        GlueNative::LoadInterface();
        EventRegisterEx(EVENT_ID_UPDATE, Update, &root, EVENT_PRIORITY_NORMAL);
        EventRegisterEx(EVENT_ID_PAINT, Paint, &root, EVENT_PRIORITY_NORMAL);
        uint32_t frames = 0; uint32_t previous = OsGetAsyncTimeMs();
        while (!EventNative::Closing() && (!frameLimit || frames < frameLimit)) {
            context.Window().Pump(); OSEVENT event;
            while (OsQueueGet(&event.id, &event.param[0], &event.param[1], &event.param[2], &event.param[3])) {
                if (event.id == OS_INPUT_CLOSE || event.id == OS_INPUT_SHUTDOWN) EventPostClose();
                else if (!Frame::Native::DispatchWindowEvent(context, event)) Frame::Native::DispatchInputEvent(context, event);
            }
            const auto now = OsGetAsyncTimeMs();
            EventNative::Tick(float(now - previous) * 0.001f, now); previous = now; ++frames;
            SDL_Delay(1);
        }
        EventUnregister(EVENT_ID_UPDATE, Update); EventUnregister(EVENT_ID_PAINT, Paint);
        std::cout << "startup-frames=" << frames << std::endl;
    }
    scripts.CheckCanClose(); renderer.CheckCanClose(); fonts.CheckCanClose();
}
}
int main(int argc, char** argv) {
    try {
        std::string data, settings, cache; uint32_t frames = 0; bool hidden = false;
        for (int i = 1; i < argc; ++i) {
            std::string option = argv[i];
            if (option == "--hidden") { hidden = true; continue; }
            if (i + 1 == argc) throw std::invalid_argument("Missing option value");
            std::string value = argv[++i];
            if (option == "--data") data = value;
            else if (option == "--settings") settings = value;
            else if (option == "--cache") cache = value;
            else if (option == "--frames") { size_t used; auto count = std::stoul(value, &used); if (used != value.size() || count > 10000000) throw std::invalid_argument("Invalid frame limit"); frames = uint32_t(count); }
            else throw std::invalid_argument("Unknown native client option: " + option);
        }
        if (data.empty() || settings.empty() || cache.empty()) throw std::invalid_argument("Supply separate --data, --settings and --cache roots");
        Blizzard::File::Native::Paths paths;
        if (!Blizzard::File::Native::ConfigurePaths(data.c_str(), settings.c_str(), cache.c_str(), paths))
            throw std::invalid_argument("Data/settings/cache roots must be separate");
        std::filesystem::current_path(paths.settings);
        Runtime runtime;
        Run(paths, frames, hidden);
        if (OsLiveThreadCount()) throw std::runtime_error("Native startup left workers alive");
        std::cout << "startup-shutdown=complete workers=0" << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WowNativeClient stopped: " << error.what() << '\n';
        return 1;
    }
}
