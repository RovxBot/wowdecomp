// Original root pixel rounding, shared by Windows and native font strings.
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Tempest/CMath.h"
#include <cmath>
// VERIFY: a9f9f36d auto
// OFFSET: 0x00493D70
float CSimpleTop::RoundToPixelWidth(float ddcWidth) {
    if (fabs(ddcWidth) < 0.00000023841858f) {
        return ddcWidth;
    }

    CRect windowRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSizeInScreenCoords(windowRect);

    float windowWidth = windowRect.maxX - windowRect.minX;

    if (fabs(windowWidth) < 0.00000023841858f) {
        return ddcWidth;
    }

    int32_t pixels = static_cast<int32_t>(CMath::fuint_pi(DDCToNDCWidth(ddcWidth) * windowWidth));

    return NDCToDDCWidth(pixels / windowWidth);
}

// VERIFY: c3d69b78 auto
// OFFSET: 0x00493E00
float CSimpleTop::RoundToPixelHeight(float ddcHeight) {
    if (fabs(ddcHeight) < 0.00000023841858f) {
        return ddcHeight;
    }

    CRect windowRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSizeInScreenCoords(windowRect);

    float windowHeight = windowRect.maxY - windowRect.minY;

    if (fabs(windowHeight) < 0.00000023841858f) {
        return ddcHeight;
    }

    int32_t pixels = static_cast<int32_t>(CMath::fuint_pi(DDCToNDCHeight(ddcHeight) * windowHeight));

    return NDCToDDCHeight(pixels / windowHeight);
}

