// Native host adaptation of the original display-size delivery order.
#include "WindowEvents.h"
#include "Hierarchy.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Gx/Portable/GlContext.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Base/Coordinate.h"
namespace Frame { namespace Native {
bool DispatchWindowEvent(Gx::Native::GlContext& context, const OSEVENT& event) {
    RequireRoot();
    RequireTreeMutation();
    context.CheckCurrent();
    if (!g_theGxDevicePtr || g_theGxDevicePtr->DeviceWindow() != context.Window().Handle())
        throw std::logic_error("Frame size delivery requires the owning Gx window");
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
}}
