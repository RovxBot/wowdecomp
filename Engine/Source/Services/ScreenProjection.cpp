// Original screen projection shared with the native Frame root.
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/Gxu.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CRect.h"
// VERIFY: 5a7761be auto
// OFFSET: 0x004BEE60
void CameraSetupScreenProjection(const CRect& rect, const C2Vector& offset, float depth, int32_t forceOrtho) {
    float centerX = (rect.minX + rect.maxX) * 0.5f;
    float centerY = 0.5f * (rect.minY + rect.maxY);
    float left = rect.minX - centerX;
    float right = rect.maxX - centerX;
    float bottom = rect.minY + -centerY;
    float top = -centerY + rect.maxY;

    C44Matrix projection;

    if (forceOrtho || !g_theGxDevicePtr->StereoEnabled()) {
        GxuXformCreateOrtho(left, right, bottom, top, -500.0f, 500.0f, projection);
        projection.Scale(C3Vector(1.0f, 1.0f, -1.0f));
    } else {
        GxuXformCreateOrthoDepth(left, right, bottom, top, -500.0f, 500.0f, projection);
    }

    CGxCaps& caps = g_theGxDevicePtr->Caps();

    if (!caps.m_pixelCenterOnEdge && caps.m_texelCenterOnEdge) {
        CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_theGxDevicePtr->CapsWindowSizeInScreenCoords(window);
        projection.d0 = projection.d0 - (rect.maxX - rect.minX) / (window.maxX - window.minX) * 0.5f;
        projection.d1 = 0.5f * ((rect.maxY - rect.minY) / (window.maxY - window.minY)) + projection.d1;
    }

    GxXformSetProjection(projection);

    C44Matrix view;
    view.Translate(C3Vector(offset.x - centerX, offset.y - centerY, 0.0f));
    GxXformSetView(view);
}

