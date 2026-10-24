#include "Engine/Source/Frame/CSimpleButton.h"
#include "Engine/Source/Event/Types.h"

// VERIFY: f5952a56 auto
// OFFSET: 0x0096F530
const char* GetMouseButtonName(uint32_t button) {
    switch (button) {
        case 0x1:
            return "LeftButton";
        case 0x2:
            return "MiddleButton";
        case 0x4:
            return "RightButton";
        case 0x8:
            return "Button4";
        case 0x10:
            return "Button5";
        case 0x20:
            return "Button6";
        case 0x40:
            return "Button7";
        case 0x80:
            return "Button8";
        case 0x100:
            return "Button9";
        case 0x200:
            return "Button10";
        case 0x400:
            return "Button11";
        case 0x800:
            return "Button12";
        case 0x1000:
            return "Button13";
        case 0x2000:
            return "Button14";
        case 0x4000:
            return "Button15";
        case MOUSE_BUTTON_XBUTTON13:
            return "Button16";
        case MOUSE_BUTTON_XBUTTON14:
            return "Button17";
        case MOUSE_BUTTON_XBUTTON15:
            return "Button18";
        case MOUSE_BUTTON_XBUTTON16:
            return "Button19";
        case MOUSE_BUTTON_XBUTTON17:
            return "Button20";
        case MOUSE_BUTTON_XBUTTON18:
            return "Button21";
        case MOUSE_BUTTON_XBUTTON19:
            return "Button22";
        case MOUSE_BUTTON_XBUTTON20:
            return "Button23";
        case MOUSE_BUTTON_XBUTTON21:
            return "Button24";
        case MOUSE_BUTTON_XBUTTON22:
            return "Button25";
        case MOUSE_BUTTON_XBUTTON23:
            return "Button26";
        case MOUSE_BUTTON_XBUTTON24:
            return "Button27";
        case MOUSE_BUTTON_XBUTTON25:
            return "Button28";
        case MOUSE_BUTTON_XBUTTON26:
            return "Button29";
        case MOUSE_BUTTON_XBUTTON27:
            return "Button30";
        case MOUSE_BUTTON_XBUTTON28:
            return "Button31";
        default:
            return "UNKNOWN";
    }
}

