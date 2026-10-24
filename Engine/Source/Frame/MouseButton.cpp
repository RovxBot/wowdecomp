#include "Engine/Source/Frame/CSimpleButton.h"

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
        case 0x8000:
            return "Button16";
        case 0x10000:
            return "Button17";
        case 0x20000:
            return "Button18";
        case 0x40000:
            return "Button19";
        case 0x80000:
            return "Button20";
        case 0x100000:
            return "Button21";
        case 0x200000:
            return "Button22";
        case 0x400000:
            return "Button23";
        case 0x800000:
            return "Button24";
        case 0x1000000:
            return "Button25";
        case 0x2000000:
            return "Button26";
        case 0x4000000:
            return "Button27";
        case 0x8000000:
            return "Button28";
        case 0x10000000:
            return "Button29";
        case 0x20000000:
            return "Button30";
        case 0x40000000:
            return "Button31";
        default:
            return "UNKNOWN";
    }
}

