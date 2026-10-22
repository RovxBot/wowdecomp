// Native host adaptation of the original display-size delivery order.
#include "WindowEvents.h"
#include "Hierarchy.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Gx/Portable/GlContext.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Base/Coordinate.h"
namespace Frame { namespace Native {
namespace {
void CheckWindow(Gx::Native::GlContext& context) {
    RequireRoot();
    RequireTreeMutation();
    context.CheckCurrent();
    if (!g_theGxDevicePtr || g_theGxDevicePtr->DeviceWindow() != context.Window().Handle())
        throw std::logic_error("Frame delivery requires the owning Gx window");
}
}
bool DispatchWindowEvent(Gx::Native::GlContext& context, const OSEVENT& event) {
    CheckWindow(context);
    if (event.id != OS_INPUT_SIZE) return false;
    int width, height, pixelWidth, pixelHeight;
    context.Window().Size(width, height, pixelWidth, pixelHeight);
    if (width <= 0 || height <= 0 || pixelWidth <= 0 || pixelHeight <= 0) return true;
    // Gx refreshes the drawable viewport without replacing a bound offscreen
    // target. Gxu/layout retain logical-pixel metrics, as in CapsWindowSize.
    g_theGxDevicePtr->DeviceWM(GxWM_Size, 0, 0);
    CoordinateSetAspectRatio(float(width) / float(height));
    EVENT_DATA_SIZE size{width, height};
    CSimpleTop::OnDisplaySizeChanged(&size, CSimpleTop::s_instance);
    return true;
}
InputResult DispatchInputEvent(Gx::Native::GlContext& context, const OSEVENT& event) {
    CheckWindow(context);
    auto& root = *CSimpleTop::s_instance;
    int32_t propagate;
    if (event.id == OS_INPUT_CAPTURE_CHANGED) {
        root.CancelMouseInput();
        return InputResult::Handled;
    } else if (event.id == OS_INPUT_MOUSE_MOVE || event.id == OS_INPUT_MOUSE_MOVE_RELATIVE ||
               event.id == OS_INPUT_MOUSE_DOWN || event.id == OS_INPUT_MOUSE_UP || event.id == OS_INPUT_MOUSE_WHEEL) {
        const bool down = event.id == OS_INPUT_MOUSE_DOWN, up = event.id == OS_INPUT_MOUSE_UP;
        const bool relative = event.id == OS_INPUT_MOUSE_MOVE_RELATIVE;
        const auto button = uint32_t(event.param[0]);
        if ((down || up) && (!button || (button & (button - 1)) || button > MOUSE_BUTTON_XBUTTON2))
            throw std::invalid_argument("Invalid native Frame mouse button");
        if (!down && !up && event.id != OS_INPUT_MOUSE_WHEEL && button)
            throw std::invalid_argument("Invalid native Frame mouse motion");
        int width, height, pixelWidth, pixelHeight;
        context.Window().Size(width, height, pixelWidth, pixelHeight);
        if (width <= 0 || height <= 0) return InputResult::Unhandled;
        EVENT_DATA_MOUSE data{};
        data.m_mode = (relative || context.Window().IsRelative()) ? MOUSE_MODE_RELATIVE : MOUSE_MODE_NORMAL;
        data.m_flags = data.m_mode == MOUSE_MODE_RELATIVE ? 2 : 0;
        data.m_button = static_cast<MOUSEBUTTON>((down || up) ? button : 0);
        if (down) root.m_nativeButtonsDown |= button;
        if (up) root.m_nativeButtonsDown &= ~button;
        data.m_buttonState = root.m_nativeButtonsDown;
        data.m_metaKeyState = root.m_nativeMetaKeys;
        data.m_time = uint32_t(event.param[3]);
        // Match original ConvertPosition: logical pixels, inverted Y, no clamp.
        // Original relative events carry raw deltas through CMouseEvent's DDC conversion.
        data.m_x = relative ? float(event.param[1]) : float(event.param[1]) / width;
        data.m_y = relative ? float(event.param[2]) : 1.0f - float(event.param[2]) / height;
        if (event.id == OS_INPUT_MOUSE_WHEEL) data.m_wheelDistance = event.param[0];
        if (!relative) root.m_nativeMouseSeen = true;
        if (relative) propagate = CSimpleTop::OnMouseMoveRelative(&data, &root);
        else if (down) propagate = CSimpleTop::OnMouseDown(&data, &root);
        else if (up) propagate = CSimpleTop::OnMouseUp(&data, &root);
        else if (event.id == OS_INPUT_MOUSE_WHEEL) propagate = CSimpleTop::OnMouseWheel(&data, &root);
        else propagate = CSimpleTop::OnMouseMove(&data, &root);
    } else if (event.id == OS_INPUT_CHAR) {
        const auto scalar = uint32_t(event.param[0]);
        if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff) || event.param[1] < 1)
            throw std::invalid_argument("Invalid Frame committed text event");
        EVENT_DATA_CHAR data{event.param[0], root.m_nativeMetaKeys, uint32_t(event.param[1])};
        propagate = CSimpleTop::OnChar(&data, &root);
    } else if (event.id == OS_INPUT_IME) {
        // OsQueueGet installs this event's retained composition snapshot. Deliver
        // immediately, before another dequeue replaces the OsIME getter state.
        if (event.param[0] == 0) return InputResult::Unsupported;
        EVENT_DATA_IME data{event.param[0], event.param[1], event.param[2], 65001};
        propagate = CSimpleTop::OnIme(&data, &root);
    } else if (event.id == OS_INPUT_KEY_DOWN || event.id == OS_INPUT_KEY_UP) {
        const auto index = uint32_t(event.param[0]);
        if (index >= KEY_LAST || event.param[1] < 1)
            throw std::invalid_argument("Invalid Frame key event");
        const bool down = event.id == OS_INPUT_KEY_DOWN;
        const bool repeated = down && root.m_nativeKeysDown[index];
        root.m_nativeKeysDown[index] = down;
        // Preserve original six modifier bits. Native Super keys are bounded
        // keys, not aliases for the original Alt/Control/Shift bits.
        if (index <= KEY_LASTMETAKEY) {
            if (repeated) return InputResult::Unhandled;
            if (down) root.m_nativeMetaKeys |= 1u << index;
            else root.m_nativeMetaKeys &= ~(1u << index);
        }
        EVENT_DATA_KEY data{static_cast<KEY>(index), root.m_nativeMetaKeys,
                            uint32_t(event.param[1]), uint32_t(event.param[3])};
        propagate = down ? (repeated ? CSimpleTop::OnKeyDownRepeat(&data, &root)
                                    : CSimpleTop::OnKeyDown(&data, &root))
                         : CSimpleTop::OnKeyUp(&data, &root);
    } else if (event.id == OS_INPUT_FOCUS) {
        if (event.param[0] != 0 && event.param[0] != 1)
            throw std::invalid_argument("Invalid Frame focus event");
        root.CancelMouseInput();
        // Window::Pump queues held-key releases before focus loss. Also retire
        // any remaining captures for direct host focus delivery or missed input.
        for (auto& key : root.m_nativeKeysDown) key = false;
        for (auto& capture : root.m_keydownCapture) capture = nullptr;
        root.m_nativeMetaKeys = 0;
        EVENT_DATA_FOCUS data{event.param[0]};
        propagate = CSimpleTop::OnFocusChanged(&data, &root);
    } else return InputResult::Unsupported;
    return propagate ? InputResult::Unhandled : InputResult::Handled;
}
}}
