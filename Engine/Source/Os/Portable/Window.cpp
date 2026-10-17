#include "Window.h"
#include "Engine/Source/Os/OsClipboard.h"
#include "Engine/Source/Os/OsIME.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#if defined(WOWDECOMP_OS_MACOS)
#include <pthread.h>
#elif defined(WOWDECOMP_OS_LINUX)
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace OsNative {
std::vector<uint32_t> DecodeUtf8(const std::string &text) {
    std::vector<uint32_t> result;
    for (size_t i = 0; i < text.size();) {
        uint8_t first = static_cast<uint8_t>(text[i++]);
        uint32_t value;
        unsigned tail;
        if (first < 128) {
            value = first;
            tail = 0;
        } else if (first >= 0xc2 && first <= 0xdf) {
            value = first & 31;
            tail = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            value = first & 15;
            tail = 2;
        } else if (first >= 0xf0 && first <= 0xf4) {
            value = first & 7;
            tail = 3;
        } else
            throw std::invalid_argument("Invalid UTF-8 input leader");
        if (tail > text.size() - i)
            throw std::invalid_argument("Truncated UTF-8 input");
        for (unsigned j = 0; j < tail; ++j) {
            uint8_t next = static_cast<uint8_t>(text[i++]);
            if ((next & 0xc0) != 0x80)
                throw std::invalid_argument("Invalid UTF-8 input continuation");
            value = (value << 6) | (next & 63);
        }
        if ((tail == 1 && value < 128) || (tail == 2 && value < 2048) || (tail == 3 && value < 65536) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) || !value)
            throw std::invalid_argument("Invalid Unicode input scalar");
        result.push_back(value);
    }
    return result;
}
std::string EncodeUtf8(uint32_t value) {
    if (!value || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        throw std::invalid_argument("Invalid Unicode scalar");
    if (value < 128)
        return std::string(1, static_cast<char>(value));
    if (value < 2048)
        return {static_cast<char>(0xc0 | value >> 6), static_cast<char>(0x80 | (value & 63))};
    if (value < 65536)
        return {static_cast<char>(0xe0 | value >> 12), static_cast<char>(0x80 | ((value >> 6) & 63)),
                static_cast<char>(0x80 | (value & 63))};
    return {static_cast<char>(0xf0 | value >> 18), static_cast<char>(0x80 | ((value >> 12) & 63)),
            static_cast<char>(0x80 | ((value >> 6) & 63)), static_cast<char>(0x80 | (value & 63))};
}
struct Envelope {
    OSEVENT event;
    Composition composition;
};
struct WindowState {
    SDL_Window *window = nullptr;
    std::thread::id owner = std::this_thread::get_id();
    std::mutex mutex;
    std::deque<Envelope> events;
    std::array<bool, KEY_LAST> keys{};
    std::array<bool, KEY_LAST> physicalKeys{};
    uint32_t buttons = 0, physicalButtons = 0, imeEnables = 0;
    int mouseX = 0, mouseY = 0, anchorX = 0, anchorY = 0;
    float mouseSpeed = 1;
    double remainderX = 0, remainderY = 0, wheel = 0;
    bool relative = false, focused = true;
    Composition editing, current;
    void (*pollProc)() = nullptr;
    ~WindowState() {
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }
};
namespace {
WindowState *active = nullptr;
WindowState &State() {
    if (!active || std::this_thread::get_id() != active->owner)
        throw std::logic_error("SDL window/input require the process main thread");
    return *active;
}
void Push(OSINPUT id, int a = 0, int b = 0, int c = 0) {
    auto &state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.events.push_back({{id, {a, b, c, static_cast<int32_t>(OsGetAsyncTimeMs())}}, state.editing});
}
int Coordinate(float value) {
    if (!std::isfinite(value) || value < -2147483000.0f || value > 2147483000.0f)
        throw std::runtime_error("Invalid SDL mouse coordinate");
    return static_cast<int>(std::lround(value));
}
KEY Key(const SDL_KeyboardEvent &key) {
    if (key.key >= SDLK_A && key.key <= SDLK_Z)
        return static_cast<KEY>(key.key - SDLK_A + KEY_A);
    if ((key.key >= 32 && key.key <= 126) && !(key.key & SDLK_SCANCODE_MASK)) {
        if (key.key >= 'a' && key.key <= 'z')
            return static_cast<KEY>(key.key - 'a' + KEY_A);
        return static_cast<KEY>(key.key);
    }
    if (key.scancode >= SDL_SCANCODE_F1 && key.scancode <= SDL_SCANCODE_F12)
        return static_cast<KEY>(KEY_F1 + key.scancode - SDL_SCANCODE_F1);
    if (key.scancode >= SDL_SCANCODE_KP_1 && key.scancode <= SDL_SCANCODE_KP_9)
        return static_cast<KEY>(KEY_NUMPAD1 + key.scancode - SDL_SCANCODE_KP_1);
    switch (key.scancode) {
    case SDL_SCANCODE_LSHIFT:
        return KEY_LSHIFT;
    case SDL_SCANCODE_RSHIFT:
        return KEY_RSHIFT;
    case SDL_SCANCODE_LCTRL:
        return KEY_LCONTROL;
    case SDL_SCANCODE_RCTRL:
        return KEY_RCONTROL;
    case SDL_SCANCODE_LALT:
        return KEY_LALT;
    case SDL_SCANCODE_RALT:
        return KEY_RALT;
    case SDL_SCANCODE_LGUI:
        return KEY_LSUPER;
    case SDL_SCANCODE_RGUI:
        return KEY_RSUPER;
    case SDL_SCANCODE_ESCAPE:
        return KEY_ESCAPE;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return KEY_ENTER;
    case SDL_SCANCODE_BACKSPACE:
        return KEY_BACKSPACE;
    case SDL_SCANCODE_TAB:
        return KEY_TAB;
    case SDL_SCANCODE_LEFT:
        return KEY_LEFT;
    case SDL_SCANCODE_RIGHT:
        return KEY_RIGHT;
    case SDL_SCANCODE_UP:
        return KEY_UP;
    case SDL_SCANCODE_DOWN:
        return KEY_DOWN;
    case SDL_SCANCODE_INSERT:
        return KEY_INSERT;
    case SDL_SCANCODE_DELETE:
        return KEY_DELETE;
    case SDL_SCANCODE_HOME:
        return KEY_HOME;
    case SDL_SCANCODE_END:
        return KEY_END;
    case SDL_SCANCODE_PAGEUP:
        return KEY_PAGEUP;
    case SDL_SCANCODE_PAGEDOWN:
        return KEY_PAGEDOWN;
    case SDL_SCANCODE_CAPSLOCK:
        return KEY_CAPSLOCK;
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return KEY_NUMLOCK;
    case SDL_SCANCODE_SCROLLLOCK:
        return KEY_SCROLLLOCK;
    case SDL_SCANCODE_PAUSE:
        return KEY_PAUSE;
    case SDL_SCANCODE_PRINTSCREEN:
        return KEY_PRINTSCREEN;
    case SDL_SCANCODE_KP_0:
        return KEY_NUMPAD0;
    case SDL_SCANCODE_KP_PLUS:
        return KEY_NUMPAD_PLUS;
    case SDL_SCANCODE_KP_MINUS:
        return KEY_NUMPAD_MINUS;
    case SDL_SCANCODE_KP_MULTIPLY:
        return KEY_NUMPAD_MULTIPLY;
    case SDL_SCANCODE_KP_DIVIDE:
        return KEY_NUMPAD_DIVIDE;
    case SDL_SCANCODE_KP_PERIOD:
        return KEY_NUMPAD_DECIMAL;
    case SDL_SCANCODE_KP_EQUALS:
        return KEY_NUMPAD_EQUALS;
    default:
        return KEY_NONE;
    }
}
MOUSEBUTTON Button(uint8_t button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MOUSE_BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE:
        return MOUSE_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT:
        return MOUSE_BUTTON_RIGHT;
    case SDL_BUTTON_X1:
        return MOUSE_BUTTON_XBUTTON1;
    case SDL_BUTTON_X2:
        return MOUSE_BUTTON_XBUTTON2;
    default:
        return MOUSE_BUTTON_NONE;
    }
}
void ReleaseInput(WindowState &state) {
    for (uint32_t i = 0; i < state.physicalKeys.size(); ++i)
        if (state.physicalKeys[i]) {
            state.physicalKeys[i] = false;
            Push(OS_INPUT_KEY_UP, i, 1);
        }
    for (uint32_t button = 1; button <= MOUSE_BUTTON_XBUTTON2; button <<= 1)
        if (state.physicalButtons & button)
            Push(OS_INPUT_MOUSE_UP, button, state.mouseX, state.mouseY);
    state.physicalButtons = 0;
    if (state.relative) {
        if (!SDL_SetWindowRelativeMouseMode(state.window, false))
            throw std::runtime_error(SDL_GetError());
        state.relative = false;
    }
    if (!state.editing.text.empty()) {
        state.editing = {};
        Push(OS_INPUT_IME, 6);
    }
    Push(OS_INPUT_CAPTURE_CHANGED, 0, state.mouseX, state.mouseY);
}
void Translate(const SDL_Event &event) {
    auto &state = State();
    if (event.type == SDL_EVENT_QUIT) {
        Push(OS_INPUT_CLOSE);
        return;
    }
    SDL_WindowID id = 0;
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
        id = event.window.windowID;
    else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
        id = event.key.windowID;
    else if (event.type == SDL_EVENT_TEXT_INPUT)
        id = event.text.windowID;
    else if (event.type == SDL_EVENT_TEXT_EDITING)
        id = event.edit.windowID;
    else if (event.type == SDL_EVENT_TEXT_EDITING_CANDIDATES)
        id = event.edit_candidates.windowID;
    else if (event.type == SDL_EVENT_MOUSE_MOTION)
        id = event.motion.windowID;
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        id = event.button.windowID;
    else if (event.type == SDL_EVENT_MOUSE_WHEEL)
        id = event.wheel.windowID;
    else
        return;
    if (id != SDL_GetWindowID(state.window))
        return;
    switch (event.type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        Push(OS_INPUT_CLOSE);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        Push(OS_INPUT_SIZE, event.window.data1, event.window.data2);
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        int w, h;
        if (!SDL_GetWindowSize(state.window, &w, &h))
            throw std::runtime_error(SDL_GetError());
        Push(OS_INPUT_SIZE, w, h);
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        state.focused = true;
        Push(OS_INPUT_FOCUS, 1);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        state.focused = false;
        ReleaseInput(state);
        Push(OS_INPUT_FOCUS, 0);
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        KEY key = Key(event.key);
        if (key == KEY_NONE)
            break;
        state.physicalKeys[key] = event.type == SDL_EVENT_KEY_DOWN;
        Push(event.type == SDL_EVENT_KEY_DOWN ? OS_INPUT_KEY_DOWN : OS_INPUT_KEY_UP, key, 1);
        break;
    }
    case SDL_EVENT_TEXT_EDITING: {
        std::string text = event.edit.text ? event.edit.text : "";
        DecodeUtf8(text);
        bool wasEditing = !state.editing.text.empty();
        state.editing.text = std::move(text);
        state.editing.result.clear();
        state.editing.start = event.edit.start;
        state.editing.length = event.edit.length;
        if (!wasEditing && !state.editing.text.empty())
            Push(OS_INPUT_IME, 1);
        Push(OS_INPUT_IME, 2, 0, 3);
        if (state.editing.text.empty())
            Push(OS_INPUT_IME, 6);
        break;
    }
    case SDL_EVENT_TEXT_INPUT: {
        std::string text = event.text.text ? event.text.text : "";
        auto scalars = DecodeUtf8(text);
        if (!state.editing.text.empty()) {
            state.editing.result = std::move(text);
            state.editing.text.clear();
            Push(OS_INPUT_IME, 2, 0, 4);
            Push(OS_INPUT_IME, 6);
            state.editing = {};
        } else
            for (auto scalar : scalars)
                if (scalar >= 32)
                    Push(OS_INPUT_CHAR, scalar, 1);
        break;
    }
    case SDL_EVENT_TEXT_EDITING_CANDIDATES: {
        bool previous = !state.editing.candidates.empty();
        state.editing.candidates.clear();
        if (event.edit_candidates.num_candidates < 0 || event.edit_candidates.num_candidates > 1024)
            throw std::runtime_error("Invalid IME candidate count");
        for (int i = 0; i < event.edit_candidates.num_candidates; ++i) {
            std::string candidate = event.edit_candidates.candidates[i];
            DecodeUtf8(candidate);
            state.editing.candidates.push_back(std::move(candidate));
        }
        state.editing.selected = event.edit_candidates.selected_candidate;
        Push(OS_INPUT_IME, state.editing.candidates.empty() ? 5 : previous ? 4 : 3);
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        auto button = Button(event.button.button);
        if (!button)
            break;
        state.mouseX = Coordinate(event.button.x);
        state.mouseY = Coordinate(event.button.y);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            state.physicalButtons |= button;
        else
            state.physicalButtons &= ~button;
        Push(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? OS_INPUT_MOUSE_DOWN : OS_INPUT_MOUSE_UP, button,
             state.mouseX, state.mouseY);
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (state.relative) {
            state.remainderX += event.motion.xrel * state.mouseSpeed;
            state.remainderY += event.motion.yrel * state.mouseSpeed;
            int x = Coordinate(static_cast<float>(state.remainderX)),
                y = Coordinate(static_cast<float>(state.remainderY));
            state.remainderX -= x;
            state.remainderY -= y;
            if (x || y)
                Push(OS_INPUT_MOUSE_MOVE_RELATIVE, 0, x, y);
        } else {
            state.mouseX = Coordinate(event.motion.x);
            state.mouseY = Coordinate(event.motion.y);
            Push(OS_INPUT_MOUSE_MOVE, 0, state.mouseX, state.mouseY);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        double direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
        state.wheel += event.wheel.y * direction * 120;
        int amount = Coordinate(static_cast<float>(state.wheel));
        state.wheel -= amount;
        if (amount)
            Push(OS_INPUT_MOUSE_WHEEL, amount, Coordinate(event.wheel.mouse_x),
                 Coordinate(event.wheel.mouse_y));
        break;
    }
    default:
        break;
    }
}
} // namespace
Window::Window(const char *title, int width, int height, bool hidden) {
    if (active)
        throw std::logic_error("Only one native shell window may own Os input");
#if defined(WOWDECOMP_OS_MACOS)
    if (!pthread_main_np())
        throw std::logic_error("Cocoa requires the process main thread");
#elif defined(WOWDECOMP_OS_LINUX)
    if (syscall(SYS_gettid) != getpid())
        throw std::logic_error("SDL shell requires the process main thread");
#endif
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO))
        throw std::runtime_error(SDL_GetError());
    state = std::make_unique<WindowState>();
    state->window = SDL_CreateWindow(title, width, height,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                         (hidden ? SDL_WINDOW_HIDDEN : 0));
    if (!state->window)
        throw std::runtime_error(SDL_GetError());
    active = state.get();
    OsInputInitialize();
    OsIMEInitialize();
}
Window::~Window() {
    if (std::this_thread::get_id() != state->owner)
        std::terminate();
    OsInputDestroy();
    active = nullptr;
}
SDL_Window *Window::Handle() const {
    State();
    return state->window;
}
void Window::Pump() {
    State();
    SDL_Event event;
    while (SDL_PollEvent(&event))
        Translate(event);
    if (state->pollProc)
        state->pollProc();
}
bool Window::RelativeMouse(bool enabled) {
    State();
    if (enabled == state->relative)
        return true;
    if (enabled) {
        state->anchorX = state->mouseX;
        state->anchorY = state->mouseY;
    } else
        SDL_WarpMouseInWindow(state->window, static_cast<float>(state->anchorX),
                              static_cast<float>(state->anchorY));
    if (!SDL_SetWindowRelativeMouseMode(state->window, enabled)) {
        SErrSetLastError(50);
        return false;
    }
    state->relative = enabled;
    state->remainderX = state->remainderY = 0;
    if (!enabled) {
        state->mouseX = state->anchorX;
        state->mouseY = state->anchorY;
    }
    return true;
}
bool Window::Fullscreen(bool enabled) {
    State();
    return SDL_SetWindowFullscreen(state->window, enabled);
}
bool Window::IsRelative() const {
    State();
    return state->relative;
}
void Window::Size(int &w, int &h, int &pw, int &ph) const {
    State();
    if (!SDL_GetWindowSize(state->window, &w, &h) || !SDL_GetWindowSizeInPixels(state->window, &pw, &ph))
        throw std::runtime_error(SDL_GetError());
}
void Window::TextArea(int x, int y, int w, int h, int cursor) {
    State();
    SDL_Rect rect{x, y, w, h};
    if (!SDL_SetTextInputArea(state->window, &rect, cursor))
        throw std::runtime_error(SDL_GetError());
}
const Composition &Window::CurrentComposition() const {
    State();
    return state->current;
}
const char *Window::VideoDriver() const {
    State();
    return SDL_GetCurrentVideoDriver();
}
} // namespace OsNative

// Existing engine-facing Os interfaces, selected only for the native ABI.
int32_t OsQueueGet(OSINPUT *id, int32_t *a, int32_t *b, int32_t *c, int32_t *time) {
    auto &state = OsNative::State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.events.empty())
        return 0;
    auto event = std::move(state.events.front());
    state.events.pop_front();
    state.current = std::move(event.composition);
    auto key = static_cast<uint32_t>(event.event.param[0]);
    if (event.event.id == OS_INPUT_KEY_DOWN && key < state.keys.size())
        state.keys[key] = true;
    if (event.event.id == OS_INPUT_KEY_UP && key < state.keys.size())
        state.keys[key] = false;
    if (event.event.id == OS_INPUT_MOUSE_DOWN)
        state.buttons |= key;
    if (event.event.id == OS_INPUT_MOUSE_UP)
        state.buttons &= ~key;
    *id = event.event.id;
    *a = event.event.param[0];
    *b = event.event.param[1];
    *c = event.event.param[2];
    *time = event.event.param[3];
    return 1;
}
void OsQueuePut(OSINPUT id, int32_t a, int32_t b, int32_t c, int32_t) {
    OsNative::Push(id, a, b, c);
}
int32_t OsInputGet(OSINPUT *id, int32_t *a, int32_t *b, int32_t *c, int32_t *time) {
    auto &state = OsNative::State();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (!state.events.empty())
            goto dequeue;
    }
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            OsNative::Translate(event);
        if (state.pollProc)
            state.pollProc();
    }
dequeue:
    return OsQueueGet(id, a, b, c, time);
}
int32_t OsInputIsKeyDown(KEY key) {
    auto &state = OsNative::State();
    return key < KEY_LAST && state.keys[key];
}
int32_t OsInputIsMouseButtonDown(MOUSEBUTTON button) {
    return (OsNative::State().buttons & button) != 0;
}
void OsInputSetWindowResizeLock(int32_t locked) {
    if (!SDL_SetWindowResizable(OsNative::State().window, !locked))
        throw std::runtime_error(SDL_GetError());
}
float OsInputGetMouseSpeed() {
    return OsNative::State().mouseSpeed;
}
void OsInputSetMouseSpeed(float speed) {
    if (!std::isfinite(speed)) {
        SErrSetLastError(87);
        return;
    }
    OsNative::State().mouseSpeed = std::clamp(speed, 0.1f, 2.0f);
}
void OsInputInitialize() {
    auto &state = OsNative::State();
    state.keys.fill(false);
    state.physicalKeys.fill(false);
    state.buttons = state.physicalButtons = 0;
}
void OsInputDestroy() {
    auto &state = OsNative::State();
    OsIMEDestroy();
    if (state.relative)
        SDL_SetWindowRelativeMouseMode(state.window, false);
    state.events.clear();
    state.keys.fill(false);
    state.buttons = 0;
}
void OsInputEnableSecureMode(int32_t enabled) {
    OsNative::State();
    if (enabled)
        SErrSetLastError(50);
}
void OsInputSetEventPollProc(void (*procedure)()) {
    OsNative::State().pollProc = procedure;
}
void OsInputSetMouseMode(OS_MOUSE_MODE mode) {
    auto &state = OsNative::State();
    if (mode != OS_MOUSE_MODE_NORMAL && mode != OS_MOUSE_MODE_RELATIVE) {
        SErrSetLastError(87);
        return;
    }
    bool enabled = mode == OS_MOUSE_MODE_RELATIVE;
    if (enabled == state.relative)
        return;
    if (enabled) {
        state.anchorX = state.mouseX;
        state.anchorY = state.mouseY;
    } else
        SDL_WarpMouseInWindow(state.window, state.anchorX, state.anchorY);
    if (!SDL_SetWindowRelativeMouseMode(state.window, enabled)) {
        SErrSetLastError(50);
        return;
    }
    state.relative = enabled;
    state.remainderX = state.remainderY = 0;
}
void OsInputGetMousePosition(int32_t *x, int32_t *y) {
    auto &s = OsNative::State();
    if (x)
        *x = s.mouseX;
    if (y)
        *y = s.mouseY;
}
void OsInputSetMousePosition(int32_t x, int32_t y) {
    auto &s = OsNative::State();
    SDL_WarpMouseInWindow(s.window, x, y);
    s.mouseX = x;
    s.mouseY = y;
}
char *OsClipboardGetString() {
    OsNative::State();
    char *text = SDL_GetClipboardText();
    if (!text)
        return nullptr;
    size_t n = std::strlen(text) + 1;
    auto copy = static_cast<char *>(SMemAlloc(n, __FILE__, __LINE__, 0));
    if (copy)
        std::memcpy(copy, text, n);
    SDL_free(text);
    return copy;
}
int32_t OsClipboardPutString(const char *text) {
    OsNative::State();
    if (!text) {
        SErrSetLastError(87);
        return 0;
    }
    OsNative::DecodeUtf8(text);
    if (!SDL_SetClipboardText(text)) {
        SErrSetLastError(50);
        return 0;
    }
    return 1;
}
void OsClipboardFreeString(char *text) {
    if (text)
        SMemFree(text, __FILE__, __LINE__, 0);
}
char *IOsClipboardGetString(void *) {
    return OsClipboardGetString();
}
int32_t IOsClipboardPutString(const char *text, void *) {
    return OsClipboardPutString(text);
}
namespace {
int CopyComposition(const std::string &text, char *output, uint32_t size) {
    if (!output || size <= text.size()) {
        SErrSetLastError(122);
        return 0;
    }
    std::memcpy(output, text.c_str(), text.size() + 1);
    return 1;
}
} // namespace
int32_t OsIMEGetLanguageMode() {
    OsNative::State();
    return 0;
}
int32_t OsIMEGetCompositionString(char *text, uint32_t size) {
    return CopyComposition(OsNative::State().current.text, text, size);
}
int32_t OsIMEGetCompositionResult(char *text, uint32_t size) {
    return CopyComposition(OsNative::State().current.result, text, size);
}
int32_t OsIMEGetClauseInfo(uint32_t &start, uint32_t &end, uint32_t &cursor) {
    auto &c = OsNative::State().current;
    uint32_t count = static_cast<uint32_t>(OsNative::DecodeUtf8(c.text).size());
    start = static_cast<uint32_t>(std::clamp(c.start, 0, int(count)));
    end = std::min(count, start + static_cast<uint32_t>(std::max(0, c.length)));
    cursor = start;
    return !c.text.empty();
}
void OsIMEEnable(int32_t enable) {
    auto &s = OsNative::State();
    if (enable) {
        if (!s.imeEnables && !SDL_StartTextInput(s.window))
            throw std::runtime_error(SDL_GetError());
        ++s.imeEnables;
    } else if (s.imeEnables && !--s.imeEnables) {
        if (!SDL_StopTextInput(s.window))
            throw std::runtime_error(SDL_GetError());
    }
}
void OsIMESync() {
    auto &state = OsNative::State();
    bool wanted = state.imeEnables != 0;
    if (wanted != SDL_TextInputActive(state.window)) {
        bool ok = wanted ? SDL_StartTextInput(state.window) : SDL_StopTextInput(state.window);
        if (!ok)
            throw std::runtime_error(SDL_GetError());
    }
}
void OsIMESetLanguageMode(int32_t) {
    OsNative::State();
    SErrSetLastError(50);
}
void OsIMEInitialize() {
    OsNative::State().imeEnables = 0;
}
void OsIMEDestroy() {
    auto &s = OsNative::State();
    if (s.imeEnables)
        SDL_StopTextInput(s.window);
    s.imeEnables = 0;
    s.editing = {};
    s.current = {};
}
int32_t OsIMEGetCandidates(uint32_t, uint32_t &start, uint32_t &page, uint32_t &count, uint32_t &selection,
                           TSGrowableArray<OsIMECandidate> &candidates) {
    const auto &c = OsNative::State().current;
    start = 0;
    page = count = static_cast<uint32_t>(c.candidates.size());
    selection = c.selected < 0 ? 0 : static_cast<uint32_t>(c.selected);
    candidates.SetCount(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (c.candidates[i].size() >= sizeof(candidates[i].m_text)) {
            SErrSetLastError(122);
            return 0;
        }
        std::memcpy(candidates[i].m_text, c.candidates[i].c_str(), c.candidates[i].size() + 1);
    }
    return count != 0;
}
