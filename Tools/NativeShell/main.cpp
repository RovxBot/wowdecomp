#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Os/OsClipboard.h"
#include "Engine/Source/Os/OsIME.h"
#include "Engine/Source/Os/Portable/Runtime.h"
#include "Engine/Source/Os/Portable/Window.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Services/Portable/AsyncFile.h"
#include "Engine/Source/Services/Portable/ShellSettings.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
using namespace Blizzard;
using namespace Services::Native;
struct Options {
    std::string data, locale = "enUS", settings, cache, snapshot, report;
    uint32_t smoke = 0;
};
struct Core {
    Core() {
        if (std::fesetenv(FE_DFL_ENV) || std::fesetround(FE_TONEAREST))
            throw std::runtime_error("Cannot initialize floating point");
        StormRtlInitialize();
        OsTimeStartup(OsTimeManager::BestAvailable);
    }
    ~Core() {
        OsTimeShutdown();
        StormRtlDestroy();
    }
};
struct Bundle {
    std::vector<Assets::Image> images;
    std::unique_ptr<Assets::Model> model;
    std::vector<std::string> lines;
};
ReadResult Inspect(const File::Native::Paths &paths, const std::string &locale, Bundle &bundle,
                   const Cancellation &cancel) {
    auto packages = Mopaq::Native::Packages::Open12340(paths, locale);
    bundle.lines.push_back("Original signaturefile authenticated: " +
                           std::to_string(packages->SignatureCount()) + " MD5 records");
    auto read = [&](const std::string &name) {
        if (cancel.Requested())
            throw std::runtime_error("Cancelled asset inspection");
        return packages->Read(name).bytes;
    };
    Assets::Dbc dbc(read("DBFilesClient\\Map.dbc"));
    bundle.lines.push_back("Database: Map.dbc, " + std::to_string(dbc.Rows()) + " rows");
    auto image = Assets::DecodeBlp(read("Interface\\Glues\\LoadingBar\\Loading-BarFill.blp"));
    bundle.images = Assets::DecodeBlp(read("Tileset\\Elwynn\\ElwynnDirtBase.blp"));
    bundle.lines.push_back("Textures: original UI and terrain BLPs decoded to RGBA");
    bundle.model = std::make_unique<Assets::Model>(read("Character\\Human\\Male\\HumanMale.m2"));
    auto triangles =
        Assets::DecodeSkin(read("Character\\Human\\Male\\HumanMale00.skin"), bundle.model->VertexCount());
    bundle.lines.push_back("HumanMale: " + std::to_string(bundle.model->VertexCount()) + " vertices, " +
                           std::to_string(triangles.size() / 3) + " triangles");
    auto root = Assets::DecodeWmo(
        read("World\\wmo\\Azeroth\\Buildings\\Duskwood_human_farm\\Duskwood_human_farm.wmo"));
    auto geometry = Assets::DecodeWmo(
        read("World\\wmo\\Azeroth\\Buildings\\Duskwood_human_farm\\Duskwood_human_farm_000.wmo"));
    bundle.lines.push_back("Farm WMO: " + std::to_string(root.groups) + " groups, " +
                           std::to_string(geometry.indices.size() / 3) + " inspected triangles");
    auto map = Assets::DecodeWdt(read("World\\Maps\\Azeroth\\Azeroth.wdt"));
    auto terrain = Assets::DecodeAdt(read("World\\Maps\\Azeroth\\Azeroth_32_48.adt"));
    bundle.lines.push_back("Azeroth: " + std::to_string(map.tiles.size()) + " WDT tiles, " +
                           std::to_string(terrain.size()) + " ADT cells decoded");
    return {};
}
std::string Visible(const std::string &text) {
    std::ostringstream out;
    for (auto value : OsNative::DecodeUtf8(text)) {
        if (value >= 32 && value < 127)
            out << char(value);
        else
            out << "[U+" << std::hex << std::uppercase << value << std::nouppercase << std::dec << ']';
    }
    auto result = out.str();
    if (result.size() > 105)
        result = "..." + result.substr(result.size() - 102);
    return result;
}
void Append(std::string &text, const std::string &addition) {
    if (addition.size() > 4096 - text.size())
        throw std::runtime_error("Text sample is limited to 4096 UTF-8 bytes");
    OsNative::DecodeUtf8(addition);
    text += addition;
}
Options Parse(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (i + 1 >= argc)
            throw std::runtime_error("Missing value for " + key);
        std::string value = argv[++i];
        if (key == "--data")
            options.data = value;
        else if (key == "--locale")
            options.locale = value;
        else if (key == "--settings")
            options.settings = value;
        else if (key == "--cache")
            options.cache = value;
        else if (key == "--snapshot")
            options.snapshot = value;
        else if (key == "--report")
            options.report = value;
        else if (key == "--smoke-ms") {
            size_t used = 0;
            auto number = std::stoul(value, &used);
            if (used != value.size() || number < 100 || number > 60000)
                throw std::runtime_error("Smoke duration must be 100..60000 ms");
            options.smoke = static_cast<uint32_t>(number);
        } else
            throw std::runtime_error("Unknown option: " + key);
    }
    if (options.data.empty())
        throw std::runtime_error("Pass --data with the original installation's Data directory");
    auto defaultRoot = [](const char *variable, const char *suffix) {
        const char *value = std::getenv(variable);
        if (value && *value) {
            auto path = fs::u8path(value);
            if (!path.is_absolute())
                throw std::runtime_error("User path environment must be absolute");
            return (path / "wowdecomp/native-shell").u8string();
        }
        const char *home = std::getenv("HOME");
        if (!home || !*home)
            throw std::runtime_error("Specify explicit --settings and --cache roots");
        return (fs::u8path(home) / suffix / "wowdecomp/native-shell").u8string();
    };
#if defined(WOWDECOMP_OS_LINUX)
    if (options.settings.empty())
        options.settings = defaultRoot("XDG_CONFIG_HOME", ".config");
    if (options.cache.empty())
        options.cache = defaultRoot("XDG_CACHE_HOME", ".cache");
#elif defined(WOWDECOMP_OS_MACOS)
    if (options.settings.empty())
        options.settings = defaultRoot("WOWDECOMP_NATIVE_SETTINGS_HOME", "Library/Application Support");
    if (options.cache.empty())
        options.cache = defaultRoot("WOWDECOMP_NATIVE_CACHE_HOME", "Library/Caches");
#else
    if (options.settings.empty() || options.cache.empty())
        throw std::runtime_error("Windows comparison requires explicit --settings and --cache directories");
#endif
    return options;
}
int Run(const Options &options) {
    File::Native::Paths paths;
    if (!File::Native::ConfigurePaths(options.data.c_str(), options.settings.c_str(), options.cache.c_str(),
                                      paths))
        throw std::runtime_error("Cannot configure separate data/settings/cache roots");
    auto settings = LoadSettings(paths);
    if (settings.launches == UINT64_MAX)
        throw std::runtime_error("Native shell launch counter overflow");
    ++settings.launches;
    OsNative::Window window("WoW 3.3.5a - M2 native shell", settings.width, settings.height);
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.Handle(), "software"), SDL_DestroyRenderer);
    if (!renderer)
        throw std::runtime_error(SDL_GetError());
    if (!SDL_SetRenderLogicalPresentation(renderer.get(), 1000, 700, SDL_LOGICAL_PRESENTATION_LETTERBOX))
        throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(nullptr, SDL_DestroyTexture);
    OsIMEEnable(1);
    auto bundle = std::make_shared<Bundle>();
    bool ready = false, failed = false, running = true, fullscreen = false, snapshot = false, saved = false;
    std::string status = "Opening original archives on the file worker...",
                action = "Type a sample; Unicode appears as code points in this diagnostic font.";
    uint64_t events = 0, characters = 0, ime = 0, relative = 0, focus = 0, resizes = 0, clipboard = 0;
    int pixelWidth = 0, pixelHeight = 0;
    FileQueue queue;
    auto ticket = queue.Submit(
        [paths, locale = options.locale, bundle](const Cancellation &cancel) {
            return Inspect(paths, locale, *bundle, cancel);
        },
        [&](const ReadResult &result) {
            if (result.status != ReadStatus::Success) {
                failed = true;
                status = result.status == ReadStatus::Cancelled ? "Asset loading cancelled" : result.error;
                return;
            }
            const auto &image = bundle->images[0];
            texture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                            image.width, image.height));
            if (!texture || !SDL_UpdateTexture(texture.get(), nullptr, image.rgba.data(), image.width * 4))
                throw std::runtime_error(SDL_GetError());
            ready = true;
            status = "Original 12340 assets loaded and decoded. Signature checks enabled.";
            for (const auto &line : bundle->lines)
                std::cout << line << '\n';
        });
    auto save = [&] {
        int w, h, pw, ph;
        window.Size(w, h, pw, ph);
        settings.width = std::clamp(w, 320, 8192);
        settings.height = std::clamp(h, 240, 8192);
        SaveSettings(paths, settings);
        if (!(LoadSettings(paths) == settings))
            throw std::runtime_error("Native shell settings reload differs");
        saved = true;
        action = "Settings saved and reloaded successfully.";
    };
    uint64_t start = SDL_GetTicks();
    while (running) {
        window.Pump();
        queue.Poll();
        OSEVENT event{};
        while (OsQueueGet(&event.id, &event.param[0], &event.param[1], &event.param[2], &event.param[3])) {
            ++events;
            if (event.id == OS_INPUT_CLOSE || event.id == OS_INPUT_SHUTDOWN)
                running = false;
            else if (event.id == OS_INPUT_SIZE)
                ++resizes;
            else if (event.id == OS_INPUT_FOCUS)
                ++focus;
            else if (event.id == OS_INPUT_MOUSE_MOVE_RELATIVE)
                ++relative;
            else if (event.id == OS_INPUT_CHAR) {
                Append(settings.text, OsNative::EncodeUtf8(event.param[0]));
                ++characters;
            } else if (event.id == OS_INPUT_IME) {
                ++ime;
                if (event.param[0] == 2 && (event.param[2] & 4)) {
                    char result[4097];
                    if (!OsIMEGetCompositionResult(result, sizeof(result)))
                        throw std::runtime_error("IME result exceeds input capacity");
                    Append(settings.text, result);
                }
            } else if (event.id == OS_INPUT_KEY_DOWN) {
                bool modifier = OsInputIsKeyDown(KEY_LCONTROL) || OsInputIsKeyDown(KEY_RCONTROL) ||
                                OsInputIsKeyDown(KEY_LSUPER) || OsInputIsKeyDown(KEY_RSUPER);
                auto key = static_cast<KEY>(event.param[0]);
                try {
                    if (key == KEY_ESCAPE && window.IsRelative())
                        window.RelativeMouse(false);
                    else if (key == KEY_F2) {
                        if (!window.RelativeMouse(!window.IsRelative()))
                            action = "Relative mouse unavailable: " + std::string(SDL_GetError());
                    } else if (key == KEY_F11) {
                        fullscreen = !fullscreen;
                        if (!window.Fullscreen(fullscreen)) {
                            fullscreen = !fullscreen;
                            action = "Fullscreen unavailable: " + std::string(SDL_GetError());
                        }
                    } else if (modifier && key == KEY_Q)
                        running = false;
                    else if (modifier && key == KEY_S)
                        save();
                    else if (modifier && key == KEY_C) {
                        if (!OsClipboardPutString(settings.text.c_str()))
                            throw std::runtime_error("Clipboard copy failed");
                        ++clipboard;
                        action = "Copied the text sample.";
                    } else if (modifier && key == KEY_V) {
                        auto value = OsClipboardGetString();
                        if (!value)
                            throw std::runtime_error("Clipboard paste failed");
                        std::unique_ptr<char, decltype(&OsClipboardFreeString)> owner(value,
                                                                                      OsClipboardFreeString);
                        Append(settings.text, value);
                        ++clipboard;
                        action = "Pasted the text sample.";
                    } else if (key == KEY_BACKSPACE && !settings.text.empty()) {
                        size_t start = settings.text.size() - 1;
                        while (start && (static_cast<unsigned char>(settings.text[start]) & 0xc0) == 0x80)
                            --start;
                        settings.text.resize(start);
                    }
                } catch (const std::exception &error) {
                    action = error.what();
                }
            }
        }
        int w, h;
        window.Size(w, h, pixelWidth, pixelHeight);
        // SDL renders in a logical canvas; IME candidate placement takes window
        // coordinates, including letterboxing and Retina/display scaling.
        float areaX, areaY, areaEndX, areaEndY;
        if (!SDL_RenderCoordinatesToWindow(renderer.get(), 40, 465, &areaX, &areaY) ||
            !SDL_RenderCoordinatesToWindow(renderer.get(), 950, 520, &areaEndX, &areaEndY))
            throw std::runtime_error(SDL_GetError());
        window.TextArea(static_cast<int>(std::lround(areaX)), static_cast<int>(std::lround(areaY)),
                        std::max(1, static_cast<int>(std::lround(areaEndX - areaX))),
                        std::max(1, static_cast<int>(std::lround(areaEndY - areaY))), 8);
        SDL_SetRenderDrawColor(renderer.get(), 17, 23, 33, 255);
        SDL_RenderClear(renderer.get());
        auto text = [&](float x, float y, const std::string &value, uint8_t r = 213, uint8_t g = 222,
                        uint8_t b = 233) {
            SDL_SetRenderDrawColor(renderer.get(), r, g, b, 255);
            if (!SDL_RenderDebugText(renderer.get(), x, y, value.c_str()))
                throw std::runtime_error(SDL_GetError());
        };
        SDL_SetRenderScale(renderer.get(), 2, 2);
        text(16, 14, "WoW 3.3.5a", 235, 197, 112);
        SDL_SetRenderScale(renderer.get(), 1, 1);
        text(32, 70, "M2 NATIVE APPLICATION SHELL", 141, 171, 199);
        text(32, 100, status, failed ? 245 : 145, failed ? 120 : 217, failed ? 120 : 173);
        for (size_t i = 0; ready && i < bundle->lines.size(); ++i)
            text(32, 140 + float(i) * 22, bundle->lines[i]);
        if (texture) {
            SDL_FRect preview{750, 135, 192, 192};
            if (!SDL_RenderTexture(renderer.get(), texture.get(), nullptr, &preview))
                throw std::runtime_error(SDL_GetError());
            text(750, 340, "Original terrain BLP");
        }
        text(32, 365, "NVAPI / SteelSeries / Logitech LCD: unavailable (optional)", 141, 171, 199);
        text(32, 387, "Game renderer, login, world simulation and audio: later milestones", 141, 171, 199);
        text(32, 430, "TEXT / IME / CLIPBOARD SAMPLE", 235, 197, 112);
        SDL_SetRenderDrawColor(renderer.get(), 31, 42, 58, 255);
        SDL_FRect box{28, 453, 944, 72};
        SDL_RenderFillRect(renderer.get(), &box);
        text(40, 465, Visible(settings.text));
        text(40, 495, "Composition: " + Visible(window.CurrentComposition().text), 141, 171, 199);
        text(32, 546, "Ctrl/Cmd+C copy    Ctrl/Cmd+V paste    Ctrl/Cmd+S save    Ctrl/Cmd+Q close");
        text(32, 568, "F2 relative mouse    Escape release mouse    F11 fullscreen    Window close exits");
        text(32, 598, action, 235, 197, 112);
        text(32, 630,
             "Video: " + std::string(window.VideoDriver()) + "   Window: " + std::to_string(w) + "x" +
                 std::to_string(h) + "   Pixels: " + std::to_string(pixelWidth) + "x" +
                 std::to_string(pixelHeight));
        text(32, 652,
             "Launch: " + std::to_string(settings.launches) + "   Events: " + std::to_string(events) +
                 "   Relative: " + std::to_string(relative) +
                 "   Capture: " + (window.IsRelative() ? "ON" : "OFF"));
        if (ready && !snapshot && !options.snapshot.empty()) {
            std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
                SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
            if (!surface || !SDL_SaveBMP(surface.get(), options.snapshot.c_str()))
                throw std::runtime_error(SDL_GetError());
            snapshot = true;
        }
        if (!SDL_RenderPresent(renderer.get()))
            throw std::runtime_error(SDL_GetError());
        if (options.smoke && SDL_GetTicks() - start >= options.smoke)
            running = false;
        SDL_Delay(8);
    }
    queue.Shutdown();
    save();
    if (queue.Pending() || OsLiveThreadCount())
        throw std::runtime_error("Native file worker survived shutdown");
    std::ostringstream report;
    report << "native-shell M2\nvideo=" << window.VideoDriver()
           << "\nassets=" << (ready ? "verified" : "failed")
           << "\nsettings-save-reload=" << (saved ? "verified" : "failed")
           << "\nlaunches=" << settings.launches << "\nwindow=" << settings.width << 'x' << settings.height
           << "\npixels=" << pixelWidth << 'x' << pixelHeight << "\nevents=" << events
           << "\ncharacters=" << characters << "\nime-events=" << ime << "\nrelative-events=" << relative
           << "\nfocus-events=" << focus << "\nresize-events=" << resizes
           << "\nclipboard-actions=" << clipboard << "\nworkers-after-shutdown=" << OsLiveThreadCount()
           << '\n';
    if (failed)
        report << "asset-error=" << status << '\n';
    std::cout << report.str();
    if (!options.report.empty()) {
        std::ofstream file(fs::u8path(options.report));
        file << report.str();
        file.flush();
        if (!file)
            throw std::runtime_error("Cannot write native shell runtime report");
    }
    return ready && !failed ? 0 : 1;
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "NativeShell --data ORIGINAL_DATA_DIRECTORY [--locale enUS] [--settings DIRECTORY "
                     "--cache DIRECTORY] [--smoke-ms MILLISECONDS --snapshot BMP_PATH --report TEXT_PATH]\n";
        return 0;
    }
    try {
        Core core;
        return Run(Parse(argc, argv));
    } catch (const std::exception &error) {
        std::cerr << "Native shell failed: " << error.what() << '\n';
        return 1;
    }
}
