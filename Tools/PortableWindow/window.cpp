#include "Engine/Source/Os/Portable/Window.h"
#include "Engine/Source/Os/OsClipboard.h"
#include "Engine/Source/Os/OsIME.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <thread>

void Check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::vector<OSEVENT> Drain() {
    std::vector<OSEVENT> values;
    OSEVENT e;
    while (OsInputGet(&e.id, &e.param[0], &e.param[1], &e.param[2], &e.param[3]))
        values.push_back(e);
    return values;
}
void Send(SDL_Event &event, SDL_WindowID id) {
    event.window.windowID = id;
    Check(SDL_PushEvent(&event), "SDL event injection failed");
}
int main() {
    try {
        for (size_t iteration = 0; iteration < 3; ++iteration) {
            OsNative::Window window("Native window fixture", 640, 480, true);
            auto id = SDL_GetWindowID(window.Handle());
            OsIMEEnable(1);
            window.TextArea(10, 10, 100, 20, 3);
            Drain();
            Check(OsNative::DecodeUtf8("A\xc3\xa9\xf0\x9f\x98\x80") ==
                      std::vector<uint32_t>({65, 233, 0x1f600}),
                  "UTF-8 scalar decoding failed");
            Check(OsNative::EncodeUtf8(0x1f600) == "\xf0\x9f\x98\x80", "Supplementary UTF-8 failed");
            for (auto text : {"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
                bool rejected = false;
                try {
                    OsNative::DecodeUtf8(text);
                } catch (const std::invalid_argument &) {
                    rejected = true;
                }
                Check(rejected, "Malformed UTF-8 accepted");
            }
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = SDL_SCANCODE_A;
            event.key.key = SDLK_A;
            event.key.down = true;
            Send(event, id);
            event = {};
            event.type = SDL_EVENT_KEY_UP;
            event.key.scancode = SDL_SCANCODE_A;
            event.key.key = SDLK_A;
            Send(event, id);
            window.Pump();
            OSEVENT e{};
            Check(OsQueueGet(&e.id, &e.param[0], &e.param[1], &e.param[2], &e.param[3]) &&
                      e.id == OS_INPUT_KEY_DOWN && e.param[0] == KEY_A && OsInputIsKeyDown(KEY_A),
                  "Key-down ordering/state failed");
            Check(OsQueueGet(&e.id, &e.param[0], &e.param[1], &e.param[2], &e.param[3]) &&
                      e.id == OS_INPUT_KEY_UP && !OsInputIsKeyDown(KEY_A),
                  "Key-up state failed");
            event = {};
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.text = "A\xc3\xa9\xf0\x9f\x98\x80";
            Send(event, id);
            auto text = Drain();
            Check(text.size() == 3 && text[2].id == OS_INPUT_CHAR && text[2].param[0] == 0x1f600,
                  "Text event scalar/pointer-width contract failed");
            event = {};
            event.type = SDL_EVENT_TEXT_EDITING;
            event.edit.text = "\xe4\xb8\xad";
            event.edit.start = 0;
            event.edit.length = 1;
            Send(event, id);
            event.edit.text = "\xe4\xb8\xad\xe6\x96\x87";
            event.edit.start = 1;
            Send(event, id);
            event = {};
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.text = "\xe4\xb8\xad\xe6\x96\x87";
            Send(event, id);
            window.Pump();
            size_t compositions = 0, commits = 0;
            char buffer[128];
            while (OsQueueGet(&e.id, &e.param[0], &e.param[1], &e.param[2], &e.param[3])) {
                if (e.id == OS_INPUT_IME && e.param[0] == 2 && (e.param[2] & 2)) {
                    Check(OsIMEGetCompositionString(buffer, sizeof(buffer)), "Composition getter failed");
                    Check(std::string(buffer) == (compositions ? "\xe4\xb8\xad\xe6\x96\x87" : "\xe4\xb8\xad"),
                          "Queued IME snapshot overwritten");
                    ++compositions;
                }
                if (e.id == OS_INPUT_IME && e.param[0] == 2 && (e.param[2] & 4)) {
                    Check(OsIMEGetCompositionResult(buffer, sizeof(buffer)) &&
                              std::string(buffer) == "\xe4\xb8\xad\xe6\x96\x87",
                          "IME result failed");
                    ++commits;
                }
                Check(e.id != OS_INPUT_CHAR, "IME commit duplicated as characters");
            }
            Check(compositions == 2 && commits == 1, "Composition/commit delivery count failed");
            Check(OsClipboardPutString("fixture \xc3\xa9 \xf0\x9f\x98\x80"), "Clipboard write failed");
            auto clipboard = OsClipboardGetString();
            Check(clipboard && std::string(clipboard) == "fixture \xc3\xa9 \xf0\x9f\x98\x80",
                  "Clipboard UTF-8 ownership failed");
            OsClipboardFreeString(clipboard);
            event = {};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.button = SDL_BUTTON_RIGHT;
            event.button.x = 31;
            event.button.y = 47;
            Send(event, id);
            Drain();
            Check(OsInputIsMouseButtonDown(MOUSE_BUTTON_RIGHT), "Mouse button mapping failed");
            Check(window.RelativeMouse(true), "Dummy relative mode failed");
            event = {};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.xrel = 4;
            event.motion.yrel = -3;
            Send(event, id);
            auto motion = Drain();
            bool relative = false;
            for (auto value : motion)
                if (value.id == OS_INPUT_MOUSE_MOVE_RELATIVE && value.param[1] == 4 && value.param[2] == -3)
                    relative = true;
            Check(relative, "Relative motion mapping failed");
            event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = SDL_SCANCODE_LSHIFT;
            event.key.key = SDLK_LSHIFT;
            Send(event, id);
            event = {};
            event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            Send(event, id);
            Drain();
            Check(!OsInputIsKeyDown(KEY_LSHIFT) && !OsInputIsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
                      !window.IsRelative(),
                  "Focus loss stuck input/capture");
            event = {};
            event.type = SDL_EVENT_WINDOW_RESIZED;
            event.window.data1 = 800;
            event.window.data2 = 600;
            Send(event, id);
            event = {};
            event.type = SDL_EVENT_MOUSE_WHEEL;
            event.wheel.y = 0.5f;
            event.wheel.mouse_x = 12;
            event.wheel.mouse_y = 34;
            Send(event, id);
            event = {};
            event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            Send(event, id);
            auto final = Drain();
            bool resize = false, wheel = false, close = false;
            for (auto value : final) {
                resize |= value.id == OS_INPUT_SIZE && value.param[0] == 800 && value.param[1] == 600;
                wheel |= value.id == OS_INPUT_MOUSE_WHEEL && value.param[0] == 60;
                close |= value.id == OS_INPUT_CLOSE;
            }
            Check(resize && wheel && close, "Resize/fractional wheel/close mapping failed");
            bool affinity = false;
            std::thread foreign([&] {
                try {
                    window.Pump();
                } catch (const std::logic_error &) {
                    affinity = true;
                }
            });
            foreign.join();
            Check(affinity, "Foreign SDL pump accepted");
            int w, h, pw, ph;
            window.Size(w, h, pw, ph);
            Check(w == 640 && h == 480 && pw > 0 && ph > 0, "Logical/pixel size unavailable");
        }
        std::cout
            << "SDL3 native Os input: key/focus/resize/close, UTF-8/IME snapshots, clipboard, fractional "
               "wheel, relative mouse and lifecycle passed (dummy driver; desktop hardware separate)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
