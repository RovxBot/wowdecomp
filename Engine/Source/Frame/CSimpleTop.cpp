#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Hierarchy.h"
#include <exception>
#define FRAME_TREE_CHECK() Frame::Native::RequireRegions()
#else
#define FRAME_TREE_CHECK() ((void)0)
#endif
#include "Engine/Source/Frame/CSimpleTop.h"
#include <cmath>
#include <cstring>
#include <memory>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/BLPFile/Types.h"
#include "Engine/Source/Event/CCharEvent.h"
#include "Engine/Source/Event/CFocusEvent.h"
#include "Engine/Source/Event/CImeEvent.h"
#include "Engine/Source/Event/CKeyEvent.h"
#include "Engine/Source/Event/CMouseEvent.h"
#include "Engine/Source/Event/CSizeEvent.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CFrameStrataNode.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
#include "Engine/Source/Frame/CSimpleMessageFrame.h"
#include "Engine/Source/Frame/CSimpleEditBox.h"
#endif
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Os/OsIME.h"
#include "Engine/Source/Scrn/Scrn.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CMath.h"
#include "Engine/Source/Tempest/CRect.h"

#ifdef WOWDECOMP_PORTABLE_RUNTIME
namespace {
void CheckMouseInput(const void* data, void* param) {
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame mouse callback");
    const auto* raw = static_cast<const EVENT_DATA_MOUSE*>(data);
    uint32_t mode, button;
    std::memcpy(&mode, &raw->m_mode, sizeof(mode));
    std::memcpy(&button, &raw->m_button, sizeof(button));
    if (mode >= MOUSE_MODES || button > 0x4000 || (button && (button & (button - 1))) ||
        (raw->m_buttonState & ~0x7fffu) || (raw->m_metaKeyState & ~0x3fu) ||
        (raw->m_flags & ~3u) || !std::isfinite(raw->m_x) || !std::isfinite(raw->m_y))
        throw std::invalid_argument("Invalid Frame mouse payload");
}
}
#endif

// STATIC: 0x00B499A8
CSimpleTop* CSimpleTop::s_instance;

// STATIC: 0x00B499A4
uint32_t CSimpleTop::s_eventTime;

// VERIFY: 29e70871 auto
// OFFSET: 0x00495810
#ifndef WOWDECOMP_PORTABLE_RUNTIME
static void PaintScreen(void* param, const RECTF* rect, const RECTF* visible, float elapsedSec) {
    CSimpleTop* top = CSimpleTop::s_instance;
    top->OnLayerUpdate(elapsedSec);
    top->OnLayerRender();
}

#endif

// VERIFY: 3a1b6d83 auto
// OFFSET: 0x004883B0
CSimpleTop* CSimpleTop::GetInstance() {
    FRAME_TREE_CHECK();
    return CSimpleTop::s_instance;
}

// VERIFY: 4b605abd auto
// OFFSET: 0x004883C0
void CSimpleTop::GetCursorPosition(C2Vector& position) {
    FRAME_TREE_CHECK();

    NDCToDDC(this->m_mousePosition.m_x, this->m_mousePosition.m_y, &position.x, &position.y);

}

// VERIFY: 82aafc3e auto
// OFFSET: 0x00493A10
void CSimpleTop::StopMoveOrResizeFrame() {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    auto* moving = this->m_moveResizeFrame;
    this->m_moveResizeFrame = nullptr;
    this->m_moveResizeReason = MOVERESIZE_REASON_NONE;
    this->m_nativeMoveRaisePending = false;
    // Retire the borrowed pointer before resize callbacks, including failures.
    if (moving) moving->UnflattenFrame(this);
#else

    if (this->m_moveResizeFrame) {
        this->m_moveResizeFrame->UnflattenFrame(this);
    }

    this->m_moveResizeReason = MOVERESIZE_REASON_NONE;
    this->m_moveResizeFrame = nullptr;

#endif
}

// VERIFY: eccae11a auto
// OFFSET: 0x00493C10
void CSimpleTop::NotifyFrameLayerChanged(CSimpleFrame* frame, uint32_t drawLayer) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (drawLayer >= 5) throw std::invalid_argument("Invalid root draw layer");
#endif
    CFrameStrata* strata = this->m_strata[frame->m_strata];
    strata->m_levels[frame->m_level]->m_batchDirty |= 1 << drawLayer;
    strata->m_batchDirty = 1;
}

// VERIFY: 6bdea284 auto
// OFFSET: 0x00493C50
void CSimpleTop::NotifyFrameDelayedLoad(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
    CFrameStrata* strata = this->m_strata[frame->m_strata];
    strata->m_levels[frame->m_level]->m_batchDirty |= 0x20;
    strata->m_batchDirty = 1;
}

// VERIFY: 0f832a78 auto
// OFFSET: 0x00493C80
void CSimpleTop::SetCursor(MipBits* image) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableFrameFeature();
#else

    if (image) {
        memcpy(g_theGxDevicePtr->CursorLock(), image->m_mip[0], sizeof(g_theGxDevicePtr->m_cursor));
        g_theGxDevicePtr->CursorUnlock(0, 0);
        g_theGxDevicePtr->CursorSetVisible(1);
    }

#endif
}

// VERIFY: 5547f677 auto
// OFFSET: 0x00493CE0
void CSimpleTop::MoveOrResizeFrame(const CMouseEvent& evt) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    if (!this->m_moveResizeFrame) return;
    Frame::Native::RequireSimpleFrame(this->m_moveResizeFrame);
    Frame::Native::RequireFinite(evt.m_data.m_mousePosition.x);
    Frame::Native::RequireFinite(evt.m_data.m_mousePosition.y);
#endif

    float offsetX = evt.m_data.m_mousePosition.x - this->m_moveResizeLast.x;
    float offsetY = evt.m_data.m_mousePosition.y - this->m_moveResizeLast.y;

    if (offsetX != 0.0f || offsetY != 0.0f) {
        this->m_moveResizeFrame->DragBy(offsetX, offsetY, this->m_moveResizePoint);
        this->m_moveResizeLast.x += offsetX;
        this->m_moveResizeLast.y += offsetY;
    }
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->FlushMoveRaise();
#endif

}

// VERIFY: 03c64564 auto
// OFFSET: 0x00493F30
void CSimpleTop::HideFrame(CSimpleFrame* frame, int32_t a3) {
    FRAME_TREE_CHECK();
    if (this->m_moveResizeFrame == frame && !a3) {
        this->StopMoveOrResizeFrame();
    }

    frame->UnregisterForEvents(a3);

    CFrameStrata* strata = this->m_strata[frame->m_strata];

    if (frame->m_level < strata->m_levels.Count()) {
        strata->m_batchDirty |= strata->m_levels[frame->m_level]->RemoveFrame(frame);
        strata->m_levelsDirty = 1;
    }
}

// VERIFY: 0240934e manual
// OFFSET: 0x00493FB0
void CSimpleTop::SetMouseFocus(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableFrameFeature();
#else

    CSimpleFrame* oldFocus = this->m_mouseFocus;

    if (frame != oldFocus) {
        this->m_mouseFocus = frame;

        if (oldFocus) {
            oldFocus->OnLayerCursorExit(0, 0);
        }

        if (frame) {
            frame->OnLayerCursorEnter(0);
        }
    }

    if (this->m_mouseCapture && this->m_mouseCapture != frame) {
        CMouseEvent evt;
        evt = this->m_mousePosition;
        evt.m_data.m_mousePosition.x = 0.0f;
        evt.m_data.m_mousePosition.y = 0.0f;
        evt.m_id = 0x400500C9;
        this->m_mouseCapture->OnLayerMouseUp(evt, nullptr);
        this->m_mouseCapture = nullptr;
    }

#endif
}

// VERIFY: 4a164119 manual
// OFFSET: 0x00494060
#ifdef WOWDECOMP_PORTABLE_RUNTIME
int32_t CSimpleTop::StartMoveOrResizeFrame(CSimpleFrame* frame, uint32_t reason, float x, float y, uint32_t point)
#else
int32_t CSimpleTop::StartMoveOrResizeFrame(CSimpleFrame* frame, MOVERESIZE_REASON reason, float x, float y, FRAMEPOINT point)
#endif
{
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireSimpleFrame(frame);
    Frame::Native::RequirePoint(point);
    Frame::Native::RequireFinite(x); Frame::Native::RequireFinite(y);
    if (reason < MOVERESIZE_REASON_LAYOUT || reason > MOVERESIZE_REASON_SCRIPT)
        throw std::invalid_argument("Invalid Frame movement reason");
    if (this->m_moveResizeFrame) throw std::logic_error("Stop the current Frame movement first");
    // Preflight a possible raise before flattening mutates layout anchors.
    auto* ancestor = frame;
    while (ancestor && !(ancestor->m_frameFlags & 1)) ancestor = ancestor->m_parent;
    if (ancestor && (ancestor->m_frameFlags & 0x10)) Frame::Native::RequireRaise(ancestor);
#endif

    if (!frame->FlattenFrame(this, 0.0f, 0.0f, 0.0f, 0.0f)) {
        return 0;
    }

    frame->Raise();
    frame->SetFrameFlag(0x1000, true);

    this->m_moveResizeLast.x = x;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->m_moveResizeReason = static_cast<MOVERESIZE_REASON>(reason);
#else
    this->m_moveResizeReason = reason;
#endif
    this->m_moveResizeLast.y = y;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->m_moveResizePoint = static_cast<FRAMEPOINT>(point);
#else
    this->m_moveResizePoint = point;
#endif
    this->m_moveResizeFrame = frame;

    CMouseEvent evt;
    evt = this->m_mousePosition;
    this->MoveOrResizeFrame(evt);

    return 1;

}

// VERIFY: 01703242 manual
// OFFSET: 0x00494120
#ifdef WOWDECOMP_PORTABLE_RUNTIME
int32_t CSimpleTop::StartMoveOrResizeFrame(CSimpleFrame* frame, uint32_t reason, float x, float y, int32_t resize)
#else
int32_t CSimpleTop::StartMoveOrResizeFrame(CSimpleFrame* frame, MOVERESIZE_REASON reason, float x, float y, int32_t resize)
#endif
{
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireSimpleFrame(frame);
    Frame::Native::RequireFinite(x); Frame::Native::RequireFinite(y);
    if (reason < MOVERESIZE_REASON_LAYOUT || reason > MOVERESIZE_REASON_SCRIPT || (resize != 0 && resize != 1))
        throw std::invalid_argument("Invalid Frame movement request");
#endif

    FRAMEPOINT point = FRAMEPOINT_CENTER;

    if (resize) {
        if (frame->IsResizePending()) {
            frame->Resize(1);
        }

        CRect rect = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (!frame->GetRect(&rect)) {
            return 0;
        }

        float offsetY = y - rect.minY;
        float quarterWidth = (rect.maxX - rect.minX) * 0.25f;
        float offsetX = x - rect.minX;
        float quarterHeight = (rect.maxY - rect.minY) * 0.25f;

        if (offsetX < quarterWidth) {
            if (offsetY < quarterHeight) {
                point = FRAMEPOINT_BOTTOMLEFT;
            } else if (quarterHeight * 3.0f <= offsetY) {
                point = FRAMEPOINT_TOPLEFT;
            } else {
                point = FRAMEPOINT_LEFT;
            }
        } else if (offsetX >= quarterWidth * 3.0f) {
            if (offsetY < quarterHeight) {
                point = FRAMEPOINT_BOTTOMRIGHT;
            } else if (3.0f * quarterHeight <= offsetY) {
                point = FRAMEPOINT_TOPRIGHT;
            } else {
                point = FRAMEPOINT_RIGHT;
            }
        } else if (offsetY < quarterHeight) {
            point = FRAMEPOINT_BOTTOM;
        } else if (3.0f * quarterHeight <= offsetY) {
            point = FRAMEPOINT_TOP;
        }
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    return this->StartMoveOrResizeFrame(frame, reason, x, y, uint32_t(point));
#else
    return this->StartMoveOrResizeFrame(frame, reason, x, y, point);
#endif

}

// VERIFY: d60ec4ff manual
// OFFSET: 0x00494280
int32_t CSimpleTop::OnChar(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame input callback");
    const auto scalar = uint32_t(static_cast<const EVENT_DATA_CHAR*>(data)->m_ch);
    if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) throw std::invalid_argument("Invalid Frame Unicode scalar");
    Frame::Native::ResizeScope inputGuard; // Hold consumers alive through callbacks.
#endif
    const EVENT_DATA_CHAR* charData = static_cast<const EVENT_DATA_CHAR*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    int32_t handled = 0;

    CCharEvent evt;
    evt.m_data = *charData;
    evt.m_id = EVENT_CHAR;

    for (int32_t strata = FRAME_STRATA_TOOLTIP; strata >= 0; strata--) {
        if (handled) {
            break;
        }

        CSimpleSortedArray<FRAMEPRIORITY*>& queue = top->m_eventqueue[SIMPLE_EVENT_CHAR][strata];
        queue.m_iterator = 0;

        while (queue.m_iterator < queue.m_count) {
            FRAMEPRIORITY** priority = &queue.m_array[queue.m_iterator++];

            if (!priority || handled) {
                break;
            }

            handled = (*priority)->m_frame->OnLayerChar(evt);
        }
    }

    return handled == 0;

}

// VERIFY: f2b35fa9 manual
// OFFSET: 0x00494320
int32_t CSimpleTop::OnIme(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame input callback");
    const auto& input = *static_cast<const EVENT_DATA_IME*>(data);
    // SDL supplies UTF-8 composition/candidates, not Windows ACP/language mode.
    // Validate raw action/flags before invoking any consumer or changing a queue.
    if (input.m_action < 1 || input.m_action > 6 || input.unused_0004 || input.m_codePage != 65001 ||
        (input.m_action == 2 ? input.m_param < 1 || input.m_param > 7 : input.m_param != 0))
        throw std::invalid_argument("Invalid native Frame IME payload");
    Frame::Native::ResizeScope inputGuard; // Keep consumers alive through dispatch.
#endif
    const EVENT_DATA_IME* imeData = static_cast<const EVENT_DATA_IME*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    int32_t handled = 0;

    CImeEvent evt;
    evt.m_data = *imeData;
    evt.m_id = EVENT_IME;

    for (int32_t strata = FRAME_STRATA_TOOLTIP; strata >= 0; strata--) {
        if (handled) {
            break;
        }

        CSimpleSortedArray<FRAMEPRIORITY*>& queue = top->m_eventqueue[SIMPLE_EVENT_CHAR][strata];
        queue.m_iterator = 0;

        while (queue.m_iterator < queue.m_count) {
            FRAMEPRIORITY** priority = &queue.m_array[queue.m_iterator++];

            if (!priority || handled) {
                break;
            }

            handled = (*priority)->m_frame->OnLayerIme(evt);
        }
    }

    return handled == 0;
}

// VERIFY: 81128119 manual
// OFFSET: 0x004943C0
int32_t CSimpleTop::OnKeyDown(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame input callback");
    const auto key = static_cast<const EVENT_DATA_KEY*>(data)->m_key;
    if (uint32_t(key) >= KEY_LAST) throw std::invalid_argument("Invalid Frame key index");
    Frame::Native::ResizeScope inputGuard; // Hold consumers alive through callbacks.
#endif
    const EVENT_DATA_KEY* keyData = static_cast<const EVENT_DATA_KEY*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    int32_t handled = 0;

    CKeyEvent evt;
    evt.m_data = *keyData;
    CSimpleTop::s_eventTime = evt.m_data.m_time;
    evt.m_id = EVENT_KEY_DOWN;

    if (top->m_keyDownCallback) {
        top->m_keyDownCallback(evt);
    }

    for (int32_t strata = FRAME_STRATA_TOOLTIP; strata >= 0; strata--) {
        if (handled) {
            break;
        }

        CSimpleSortedArray<FRAMEPRIORITY*>& queue = top->m_eventqueue[SIMPLE_EVENT_KEY][strata];
        queue.m_iterator = 0;

        while (queue.m_iterator < queue.m_count) {
            FRAMEPRIORITY** priority = &queue.m_array[queue.m_iterator++];

            if (!priority || handled) {
                break;
            }

            CSimpleFrame* frame = (*priority)->m_frame;
            handled = frame->OnLayerKeyDown(evt);

            if (handled) {
                top->m_keydownCapture[keyData->m_key] = frame;
            }
        }
    }

    return handled == 0;

}

// VERIFY: ab5a5ea6 manual
// OFFSET: 0x00494490
int32_t CSimpleTop::OnKeyUp(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame input callback");
    const auto key = static_cast<const EVENT_DATA_KEY*>(data)->m_key;
    if (uint32_t(key) >= KEY_LAST) throw std::invalid_argument("Invalid Frame key index");
    if (key == KEY_PRINTSCREEN && !CSimpleTop::s_instance->m_keydownCapture[key]) {
        auto result = CSimpleTop::OnKeyDown(data, param);
        CSimpleTop::s_instance->m_keydownCapture[key] = nullptr;
        return result;
    }
    Frame::Native::ResizeScope inputGuard; // Hold consumers alive through callbacks.
#endif
    const EVENT_DATA_KEY* keyData = static_cast<const EVENT_DATA_KEY*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    CSimpleFrame* frame = top->m_keydownCapture[keyData->m_key];
    int32_t handled = 0;

    CSimpleTop::s_eventTime = keyData->m_time;

    if (frame) {
        CKeyEvent evt;
        evt.m_data = *keyData;
        evt.m_id = EVENT_KEY_UP;
        frame->OnLayerKeyUp(evt);
        handled = 1;
    } else if (keyData->m_key == KEY_PRINTSCREEN) {
        handled = !CSimpleTop::OnKeyDown(data, param);
    }

    top->m_keydownCapture[keyData->m_key] = nullptr;

    return !handled;

}

// VERIFY: 060c6392 manual
// OFFSET: 0x00494530
int32_t CSimpleTop::OnKeyDownRepeat(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame input callback");
    const auto key = static_cast<const EVENT_DATA_KEY*>(data)->m_key;
    if (uint32_t(key) >= KEY_LAST) throw std::invalid_argument("Invalid Frame key index");
    Frame::Native::ResizeScope inputGuard; // Hold consumers alive through callbacks.
#endif
    const EVENT_DATA_KEY* keyData = static_cast<const EVENT_DATA_KEY*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    CSimpleFrame* frame = top->m_keydownCapture[keyData->m_key];
    int32_t handled = 0;

    if (frame) {
        CKeyEvent evt;
        evt.m_data = *keyData;
        evt.m_id = EVENT_KEY_REPEAT;
        frame->OnLayerKeyDownRepeat(evt);
        handled = 1;
    }

    return handled == 0;

}

// VERIFY: dbfbf7f9 manual
// OFFSET: 0x004945A0
int32_t CSimpleTop::OnMouseMove(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckMouseInput(data, param);
    auto* nativeTop = static_cast<CSimpleTop*>(param);
    if (nativeTop->m_moveResizeFrame) {
        const auto& position = *static_cast<const EVENT_DATA_MOUSE*>(data);
        nativeTop->m_mousePosition = position;
        CSimpleTop::s_eventTime = position.m_time;
        CMouseEvent event; event = position;
        nativeTop->MoveOrResizeFrame(event);
        return 0;
    }
    Frame::Native::ResizeScope inputGuard;
#endif

    const EVENT_DATA_MOUSE* mouseData = static_cast<const EVENT_DATA_MOUSE*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    try {
#endif
    int32_t (*callback)(CMouseEvent*) = top->m_mouseMoveCallback;

    CMouseEvent evt;
    evt = *mouseData;

    CSimpleFrame* oldFocus = top->m_mouseFocus;
    evt.m_id = 0x400500CA;

    CSimpleFrame* newFocus = nullptr;
    int32_t motion = mouseData != &top->m_mousePosition;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    const bool refresh = !top->m_nativeMouseSeen || top->m_checkFocus;
    top->m_nativeMouseSeen = true;
    top->m_checkFocus = 0;
#endif

    if (motion) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        if (!refresh && mouseData->m_x == top->m_mousePosition.m_x && mouseData->m_y == top->m_mousePosition.m_y) {
#else
        if (mouseData->m_x == top->m_mousePosition.m_x && mouseData->m_y == top->m_mousePosition.m_y) {
#endif
            return 0;
        }

        CSimpleTop::s_eventTime = mouseData->m_time;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (mouseData != &top->m_mousePosition) top->m_mousePosition = *mouseData;
#else
    memcpy(&top->m_mousePosition, mouseData, sizeof(top->m_mousePosition));
#endif

    if (top->m_moveResizeFrame) {
        CMouseEvent moveEvt;
        moveEvt = *mouseData;
        top->MoveOrResizeFrame(moveEvt);
        return 0;
    }

    if (callback && callback(&evt)) {
        return 0;
    }

    if (top->m_mouseCapture) {
        top->m_mouseCapture->OnLayerTrackUpdate(evt);
    }

    for (int32_t strata = FRAME_STRATA_TOOLTIP; strata >= 0; strata--) {
        if (newFocus) {
            break;
        }

        CSimpleSortedArray<FRAMEPRIORITY*>& queue = top->m_eventqueue[SIMPLE_EVENT_MOUSE][strata];
        queue.m_iterator = 0;

        while (queue.m_iterator < queue.m_count) {
            FRAMEPRIORITY** priority = &queue.m_array[queue.m_iterator++];

            if (!priority) {
                break;
            }

            CSimpleFrame* frame = (*priority)->m_frame;

            if (frame->OnLayerTrackUpdate(evt)) {
                newFocus = frame;
                break;
            }
        }
    }

    if (newFocus != oldFocus) {
        top->m_mouseFocus = newFocus;

        if (oldFocus) {
            oldFocus->OnLayerCursorExit(motion, 0);
        }

        if (newFocus) {
            newFocus->OnLayerCursorEnter(motion);
        }
    }

    return newFocus == nullptr;

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    } catch (...) {
        top->m_checkFocus = 1; // Retry even when the next pointer position is unchanged.
        throw;
    }
#endif
}

// VERIFY: bb6eb71e manual
// OFFSET: 0x00494730
int32_t CSimpleTop::OnMouseMoveRelative(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckMouseInput(data, param);
    Frame::Native::ResizeScope inputGuard;
#endif

    const EVENT_DATA_MOUSE* mouseData = static_cast<const EVENT_DATA_MOUSE*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    int32_t (*callback)(CMouseEvent*) = top->m_mouseMoveCallback;

    CMouseEvent evt;
    evt = *mouseData;
    evt.m_id = 0x400500CB;
    CSimpleTop::s_eventTime = mouseData->m_time;

    return !callback || !callback(&evt);

}

// VERIFY: f7cc11f1 manual
// OFFSET: 0x004947A0
int32_t CSimpleTop::OnMouseUp(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckMouseInput(data, param);
    auto* nativeTop = static_cast<CSimpleTop*>(param);
    if (nativeTop->m_moveResizeReason == MOVERESIZE_REASON_LAYOUT || nativeTop->m_moveResizeReason == MOVERESIZE_REASON_TITLE) {
        CSimpleTop::s_eventTime = static_cast<const EVENT_DATA_MOUSE*>(data)->m_time;
        nativeTop->StopMoveOrResizeFrame();
        return 0;
    }
    Frame::Native::ResizeScope inputGuard;
#endif

    const EVENT_DATA_MOUSE* mouseData = static_cast<const EVENT_DATA_MOUSE*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    CSimpleFrame* capture = top->m_mouseCapture;
    int32_t (*callback)(CMouseEvent*) = top->m_mouseButtonCallback;
    int32_t handled = 0;

    CMouseEvent evt;
    evt = *mouseData;
    CSimpleTop::s_eventTime = mouseData->m_time;
    evt.m_id = 0x400500C9;

    if (top->m_moveResizeReason == MOVERESIZE_REASON_LAYOUT || top->m_moveResizeReason == MOVERESIZE_REASON_TITLE) {
        top->StopMoveOrResizeFrame();
        return 0;
    }

    if (callback && !top->m_mouseCapture && callback(&evt)) {
        return 0;
    }

    if (capture) {
        capture->OnLayerMouseUp(evt, nullptr);
        handled = 1;

        if (!evt.m_data.m_buttonState) {
            top->m_mouseCapture = nullptr;
            return 0;
        }
    }

    return handled == 0;

}

// VERIFY: bb1ac078 manual
// OFFSET: 0x00494890
int32_t CSimpleTop::OnMouseWheel(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckMouseInput(data, param);
    Frame::Native::ResizeScope inputGuard;
#endif

    const EVENT_DATA_MOUSE* mouseData = static_cast<const EVENT_DATA_MOUSE*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    int32_t handled = 0;

    CMouseEvent evt;
    evt = *mouseData;

    C2Vector pt;
    CSimpleTop::s_eventTime = mouseData->m_time;
    evt.m_id = 0x400500CD;
    NDCToDDC(top->m_mousePosition.m_x, top->m_mousePosition.m_y, &pt.x, &pt.y);

    for (int32_t strata = FRAME_STRATA_TOOLTIP; strata >= 0; strata--) {
        if (handled) {
            break;
        }

        CSimpleSortedArray<FRAMEPRIORITY*>& queue = top->m_eventqueue[SIMPLE_EVENT_MOUSEWHEEL][strata];
        queue.m_iterator = 0;

        while (queue.m_iterator < queue.m_count) {
            FRAMEPRIORITY** priority = &queue.m_array[queue.m_iterator++];

            if (!priority || handled) {
                break;
            }

            CSimpleFrame* frame = (*priority)->m_frame;

            if (frame->TestHitRect(pt)) {
                handled = frame->OnLayerMouseWheel(evt);
            }
        }
    }

    return handled == 0;

}

// VERIFY: c36c518c manual
// OFFSET: 0x00494970
int32_t CSimpleTop::OnFocusChanged(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    Frame::Native::RequireRoot();
    if (!data || param != CSimpleTop::s_instance) throw std::invalid_argument("Invalid Frame input callback");
    Frame::Native::ResizeScope inputGuard; // Hold consumers alive through callbacks.
#endif
    const EVENT_DATA_FOCUS* focusData = static_cast<const EVENT_DATA_FOCUS*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    void (*callback)(const CFocusEvent&) = top->m_focusChangedCallback;

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    if (!focusData->m_focus && CSimpleEditBox::s_currentFocus)
        CSimpleEditBox::ClearKeyboardFocus(CSimpleEditBox::s_currentFocus, true);
#endif
    OsIMESync();

    if (callback) {
        CFocusEvent evt;
        evt.m_data = *focusData;
        evt.m_id = EVENT_WINDOW_FOCUS;
        callback(evt);
    }

    return 1;

}

// VERIFY: 3330b0b2 manual
// OFFSET: 0x00494C80
CSimpleFrame* CFrameStrata::GetHitFrame(const C2Vector& pt) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRoot();
    Frame::Native::RequireFinite(pt.x); Frame::Native::RequireFinite(pt.y);
    Frame::Native::ResizeScope traversalGuard;
#endif

    for (uint32_t level = this->m_topLevel; level > 0;) {
        level--;
        CFrameStrataNode* node = this->m_levels[level];

        for (CSimpleFrame* frame = node->m_frames.Head(); frame; frame = node->m_frames.Next(frame)) {
            if (frame->TestHitRect(pt)) {
                while (!(frame->m_frameFlags & 1)) {
                    frame = frame->m_parent;

                    if (!frame) {
                        return nullptr;
                    }
                }

                return frame;
            }
        }
    }

    return nullptr;

}

// VERIFY: 314ab723 manual
// OFFSET: 0x00494D20
int32_t CFrameStrata::FrameOccluded(CSimpleFrame* frame) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireSimpleFrame(frame);
    if (CSimpleTop::s_instance->m_strata[frame->m_strata] != this)
        throw std::invalid_argument("Frame belongs to another stratum");
    Frame::Native::ResizeScope traversalGuard;
#endif

    CRect frameRect = { 0.0f, 0.0f, 0.0f, 0.0f };
    CRect otherRect = { 0.0f, 0.0f, 0.0f, 0.0f };

    for (uint32_t level = frame->m_level; level < this->m_topLevel; level++) {
        CFrameStrataNode* node = this->m_levels[level];

        for (CSimpleFrame* other = node->m_frames.Head(); other; other = node->m_frames.Next(other)) {
            if (other == frame) {
                continue;
            }

            CSimpleFrame* parent = other->m_parent;

            while (parent && parent != frame) {
                parent = parent->m_parent;
            }

            if (parent) {
                continue;
            }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
            // Failed layout has no occluding rectangle; never reuse an old rect.
            if (!frame->GetRect(&frameRect) || !other->GetRect(&otherRect)) continue;
#else
            frame->GetRect(&frameRect);
            other->GetRect(&otherRect);
#endif
            frameRect = frameRect & otherRect;

            if (frameRect.maxY > frameRect.minY && frameRect.maxX > frameRect.minX) {
                return 1;
            }
        }
    }

    return 0;

}

// VERIFY: 8e94ced8 manual
// OFFSET: 0x00494E60
void CFrameStrata::CheckOcclusion() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRoot();
    Frame::Native::ResizeScope traversalGuard;
#endif

    for (uint32_t level = 0; level < this->m_topLevel; level++) {
        CFrameStrataNode* node = this->m_levels[level];

        for (CSimpleFrame* frame = node->m_frames.Head(); frame; frame = node->m_frames.Next(frame)) {
            if (frame->m_frameFlags & 1) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
                frame->m_frameFlags = (frame->m_frameFlags & ~0x10u) | (this->FrameOccluded(frame) ? 0x10u : 0);
#else
                frame->SetFrameFlag(0x10, this->FrameOccluded(frame));
#endif
            }
        }
    }

}

// VERIFY: 2b665fc9 manual
// OFFSET: 0x00494EE0
int32_t CFrameStrata::BuildBatches(int32_t noMouseCapture) {
    FRAME_TREE_CHECK();
    if (this->m_levelsDirty) {
        this->CheckOcclusion();
        this->m_levelsDirty = 0;
    }

    if (!this->m_batchDirty) {
        return this->m_batchDirty;
    }

    this->m_batchDirty = 0;

    for (uint32_t level = 0; level < this->m_topLevel; level++) {
        if (this->m_levels[level]->BuildBatches()) {
            this->m_batchDirty = 1;
        }
    }

    return this->m_batchDirty;
}

// VERIFY: 62b40657 manual
// OFFSET: 0x00494F30
void CFrameStrata::RenderBatches() {
    FRAME_TREE_CHECK();
    for (uint32_t level = 0; level < this->m_topLevel; level++) {
        CFrameStrataNode* node = this->m_levels[level];

        for (CRenderBatch* batch = node->m_renderList.Head(); batch; batch = node->m_renderList.Next(batch)) {
            CSimpleRender::DrawBatch(batch);
        }
    }
}

// VERIFY: df78660e auto
// OFFSET: 0x00494F90
void CSimpleTop::RegisterFrame(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
    this->m_frames.LinkToTail(frame);
}

// VERIFY: ee8d9d78 auto
// OFFSET: 0x00494FA0
void CSimpleTop::UnregisterFrame(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
    if (this->m_moveResizeFrame == frame) {
        this->StopMoveOrResizeFrame();
    }

    frame->UnregisterForEvents(0);

    CFrameStrata* strata = this->m_strata[frame->m_strata];

    if (frame->m_level < strata->m_levels.Count()) {
        strata->m_batchDirty |= strata->m_levels[frame->m_level]->RemoveFrame(frame);
        strata->m_levelsDirty = 1;
    }

    this->m_frames.UnlinkNode(frame);
}

// VERIFY: b22dcea3 auto
// OFFSET: 0x00495050
void CSimpleTop::RegisterForCleanup(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireSimpleFrame(frame);
#endif
    this->m_destroyed.LinkToTail(frame);
}

// VERIFY: cfde847a manual
// OFFSET: 0x00495060
void CSimpleTop::CompressStrata(int32_t strata) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRoot();
    Frame::Native::RequireTreeMutation();
#endif

    if (strata < FRAME_STRATA_WORLD || strata > FRAME_STRATA_TOOLTIP) {
        return;
    }

    uint32_t topLevel = this->m_strata[strata]->m_topLevel;
    int32_t emptyLevel = -1;
    uint32_t level = 0;

    while (level < topLevel) {
        int32_t used = this->m_strata[strata]->m_levels[level]->m_frames.Head() != nullptr;

        if (!used) {
            for (CSimpleFrame* frame = this->m_frames.Head(); frame; frame = this->m_frames.Next(frame)) {
                if (frame->m_strata == strata && frame->m_level == level) {
                    used = 1;
                    break;
                }
            }
        }

        if (!used) {
            if (emptyLevel == -1) {
                emptyLevel = level;
            }

            level++;
            continue;
        }

        if (emptyLevel == -1) {
            level++;
            continue;
        }

        uint32_t gap = level - emptyLevel;

        for (uint32_t moveLevel = level; moveLevel < topLevel; moveLevel++) {
            for (CSimpleFrame* frame = this->m_frames.Head(); frame; frame = this->m_frames.Next(frame)) {
                if (frame->m_strata == strata && frame->m_level == moveLevel) {
                    frame->SetFrameLevel(moveLevel - gap, 0);
                }
            }
        }

        topLevel -= gap;
        emptyLevel = -1;
    }

    if (emptyLevel != -1) {
        topLevel = emptyLevel;
    }

    this->m_strata[strata]->m_topLevel = topLevel;

}

// VERIFY: ca612c34 manual
// OFFSET: 0x004951A0
int32_t CSimpleTop::RaiseFrame(CSimpleFrame* frame, int32_t checkOcclusion) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireStateMutation();
    Frame::Native::RequireSimpleFrame(frame);
#endif

    while (!(frame->m_frameFlags & 1)) {
        frame = frame->m_parent;

        if (!frame) {
            return 0;
        }
    }

    if (checkOcclusion) {
        if (!(frame->m_layoutFlags & 0x100) || frame->IsResizePending()) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
            Frame::Native::ResizeScope resizeGuard;
#endif
            frame->Resize(1);
        }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
        frame->m_frameFlags = (frame->m_frameFlags & ~0x10u) | (this->m_strata[frame->m_strata]->FrameOccluded(frame) ? 0x10u : 0);
#else
        frame->SetFrameFlag(0x10, this->m_strata[frame->m_strata]->FrameOccluded(frame));
#endif
    }

    if (frame->m_frameFlags & 0x10) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        Frame::Native::RequireRaise(frame); // Check final descendant levels before compression mutates lists.
#endif
        this->CompressStrata(frame->m_strata);
        frame->SetFrameLevel(this->m_strata[frame->m_strata]->m_topLevel, 1);
        this->m_checkFocus = 1;
    }

    return 1;

}

// VERIFY: 36f05eee manual
// OFFSET: 0x00495270
#ifdef WOWDECOMP_PORTABLE_RUNTIME
int32_t CSimpleTop::StartMoveOrResizeFrame(const CMouseEvent& evt, uint32_t reason, int32_t resize)
#else
int32_t CSimpleTop::StartMoveOrResizeFrame(const CMouseEvent& evt, MOVERESIZE_REASON reason, int32_t resize)
#endif
{
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
    if (reason < MOVERESIZE_REASON_LAYOUT || reason > MOVERESIZE_REASON_SCRIPT || (resize != 0 && resize != 1))
        throw std::invalid_argument("Invalid Frame movement request");
    Frame::Native::RequireFinite(evt.m_data.m_mousePosition.x);
    Frame::Native::RequireFinite(evt.m_data.m_mousePosition.y);
#endif

    CSimpleFrame* frame = nullptr;

    for (int32_t strata = FRAME_STRATA_FULLSCREEN_DIALOG; strata >= 0; strata--) {
        if (frame) {
            break;
        }

        C2Vector pt;
        pt.x = evt.m_data.m_mousePosition.x;
        pt.y = evt.m_data.m_mousePosition.y;
        frame = this->m_strata[strata]->GetHitFrame(pt);
    }

    if (!frame) {
        return 0;
    }

    if (resize) {
        if (!(frame->m_frameFlags & 0x200)) {
            return 0;
        }
    } else if (!(frame->m_frameFlags & 0x100)) {
        return 0;
    }

    return this->StartMoveOrResizeFrame(frame, reason, evt.m_data.m_mousePosition.x, evt.m_data.m_mousePosition.y, resize);

}

// VERIFY: 09bbf45a auto
// OFFSET: 0x00495320
void CSimpleTop::OnLayerUpdate(float elapsedSec) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRoot();
    Frame::Native::RequireFinite(elapsedSec);
    Frame::Native::DrainDestroyQueue();
    CLayoutFrame::ResizePending();
    for (auto* item = m_frames.Head(); item; item = m_frames.Next(item))
        if (auto* edit = dynamic_cast<CSimpleEditBox*>(item)) edit->PrepareCandidates();
    CLayoutFrame::ResizePending();
    for (auto* item = m_frames.Head(); item; item = m_frames.Next(item))
        if (auto* messages = dynamic_cast<CSimpleMessageFrame*>(item)) messages->PrepareRows();
    { Frame::Native::UpdateScope scope;
#endif
    CSimpleFrame* frame;

    while ((frame = this->m_destroyed.Head()) != nullptr) {
        this->m_destroyed.UnlinkNode(frame);
        frame->AllowAttributeChanges(0);
    }

    CLayoutFrame::ResizePending();

    for (int32_t strata = 0; strata < FRAME_STRATA_TOOLTIP + 1; strata++) {
        CFrameStrata* frameStrata = this->m_strata[strata];

        for (uint32_t level = 0; level < frameStrata->m_topLevel; level++) {
            frameStrata->m_levels[level]->OnLayerUpdate(elapsedSec);
        }
    }

    CLayoutFrame::ResizePending();

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    } // End update traversal before invoking pointer consumers.
    this->FlushMoveRaise();
    if (this->m_checkFocus && this->m_nativeMouseSeen) {
        this->m_checkFocus = 0;
        CSimpleTop::OnMouseMove(&this->m_mousePosition, this);
    }
#else
    if (this->m_checkFocus) {
        this->m_checkFocus = 0;
        CSimpleTop::OnMouseMove(&this->m_mousePosition, this);
    }
#endif
}

// VERIFY: 587fb7f2 auto
// OFFSET: 0x00495410
void CSimpleTop::OnLayerRender() {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRoot();
    Frame::Native::RequireTreeMutation();
    Frame::Native::ClearDirtyBatches(*this);
    Frame::Native::CollectScope scope;
    Frame::Native::PrepareScrollBatches();
#endif
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::SetRootProjection(*this);
#else
    C2Vector offset;
    CameraSetupScreenProjection(this->m_rect, offset, 0.0f, 0);
#endif

    for (int32_t strata = 0; strata < FRAME_STRATA_TOOLTIP + 1; strata++) {
        this->m_strata[strata]->BuildBatches(this->m_mouseCapture == nullptr);
        this->m_strata[strata]->RenderBatches();
    }
}

// VERIFY: 6721d10a manual
// OFFSET: 0x00495470
int32_t CSimpleTop::OnMouseDown(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CheckMouseInput(data, param);
    const auto* mouseData = static_cast<const EVENT_DATA_MOUSE*>(data);
    auto* top = static_cast<CSimpleTop*>(param);
    if (!mouseData->m_button) throw std::invalid_argument("Missing Frame mouse button");
    if (top->m_moveResizeFrame) return 0;
    auto* frame = top->m_mouseCapture ? top->m_mouseCapture : top->m_mouseFocus;
    CMouseEvent evt; evt = *mouseData; evt.m_id = 0x400500C8;
    CSimpleTop::s_eventTime = mouseData->m_time;
    top->m_mousePosition = *mouseData;
    if (top->m_moveResizeEnabled && (top->m_nativeKeysDown[KEY_LCONTROL] || top->m_nativeKeysDown[KEY_LALT])) {
        top->StartMoveOrResizeFrame(evt, MOVERESIZE_REASON_LAYOUT, int32_t(top->m_nativeKeysDown[KEY_LCONTROL]));
        return 0;
    }
    {
        Frame::Native::ResizeScope callbackGuard;
        if (top->m_mouseButtonCallback && !top->m_mouseCapture && top->m_mouseButtonCallback(&evt)) return 0;
    }
    if (!frame) return 1;
    // No external consumer runs between guarded callback phases. Raising may
    // reorder lists here; resize callbacks hold their own traversal guard.
    frame->Raise();
    if (frame->m_titleRegion) {
        C2Vector point = evt.m_data.m_mousePosition;
        if (frame->m_titleRegion->PtInFrameRect(point)) {
            top->StartMoveOrResizeFrame(frame, MOVERESIZE_REASON_TITLE, point.x, point.y, uint32_t(FRAMEPOINT_CENTER));
            return 0;
        }
    }
    Frame::Native::ResizeScope inputGuard;
    top->m_mouseCapture = frame;
    frame->OnLayerMouseDown(evt, nullptr);
    return 0;
#else

    const EVENT_DATA_MOUSE* mouseData = static_cast<const EVENT_DATA_MOUSE*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    int32_t (*callback)(CMouseEvent*) = top->m_mouseButtonCallback;

    CMouseEvent evt;
    evt = *mouseData;
    CSimpleTop::s_eventTime = mouseData->m_time;
    evt.m_id = 0x400500C8;
    memcpy(&top->m_mousePosition, mouseData, sizeof(top->m_mousePosition));

    if (top->m_moveResizeEnabled && (EventIsKeyDown(KEY_LCONTROL) || EventIsKeyDown(KEY_LALT))) {
        top->StartMoveOrResizeFrame(evt, MOVERESIZE_REASON_LAYOUT, EventIsKeyDown(KEY_LCONTROL));
        return 0;
    }

    if (callback && !top->m_mouseCapture && callback(&evt)) {
        return 0;
    }

    CSimpleFrame* frame = top->m_mouseCapture;

    if (!frame) {
        frame = top->m_mouseFocus;

        if (!frame) {
            return 1;
        }
    }

    frame->Raise();

    if (frame->m_titleRegion) {
        C2Vector pt;
        pt.x = evt.m_data.m_mousePosition.x;
        pt.y = evt.m_data.m_mousePosition.y;

        if (frame->m_titleRegion->PtInFrameRect(pt)) {
            top->StartMoveOrResizeFrame(frame, MOVERESIZE_REASON_TITLE, evt.m_data.m_mousePosition.x, evt.m_data.m_mousePosition.y, FRAMEPOINT_CENTER);
            return 0;
        }
    }

    top->m_mouseCapture = frame;
    frame->OnLayerMouseDown(evt, nullptr);

    return 0;

#endif
}

// VERIFY: c200f27f manual
// OFFSET: 0x004955E0
int32_t CSimpleTop::OnDisplaySizeChanged(const void* data, void* param) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRoot();
    if (param != CSimpleTop::s_instance || !data) throw std::invalid_argument("Invalid native size event");
    auto* top = CSimpleTop::s_instance;
    Frame::Native::RequireTreeMutation();
    Frame::Native::ResizeScope scope;
    const CRect oldRect = top->m_rect;
    NDCToDDC(1.0f, 1.0f, &top->m_rect.maxX, &top->m_rect.maxY);
    GxuFontWindowSizeChanged();
    top->OnFrameSizeChanged(oldRect);
    for (auto* frame = top->m_frames.Head(); frame; frame = top->m_frames.Next(frame))
        frame->OnScreenSizeChanged();
    if (top->m_displaySizeCallback) {
        CSizeEvent evt;
        evt.m_data = *static_cast<const EVENT_DATA_SIZE*>(data);
        evt.m_id = 0x40040064;
        top->m_displaySizeCallback(evt);
    }
    CLayoutFrame::ResizePending();
    return 1;
#else

    const EVENT_DATA_SIZE* sizeData = static_cast<const EVENT_DATA_SIZE*>(data);
    CSimpleTop* top = static_cast<CSimpleTop*>(param);
    void (*callback)(const CSizeEvent&) = top->m_displaySizeCallback;

    GxuFontWindowSizeChanged();

    for (CSimpleFrame* frame = top->m_frames.Head(); frame; frame = top->m_frames.Next(frame)) {
        frame->OnScreenSizeChanged();
    }

    if (callback) {
        CSizeEvent evt;
        evt.m_data = *sizeData;
        evt.m_id = EVENT_WINDOW_SIZE;
        callback(evt);
    }

    return 1;

#endif
}

// VERIFY: 524e2c20 auto
// OFFSET: 0x00495840
void CSimpleTop::NotifyFrameMovedOrResized(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
    this->m_strata[frame->m_strata]->m_levelsDirty = 1;

    if (this->m_moveResizeFrame) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        // Resize callbacks cannot reorder the strata they are traversing.
        this->m_nativeMoveRaisePending = true;
#else
        this->RaiseFrame(this->m_moveResizeFrame, 0);
#endif
    }

    this->m_checkFocus = 1;
}

// VERIFY: e3307852 manual
// OFFSET: 0x00495880
#ifdef WOWDECOMP_PORTABLE_RUNTIME
void CSimpleTop::RegisterForEvent(CSimpleFrame* frame, uint32_t event, int32_t a4, int32_t priority)
#else
void CSimpleTop::RegisterForEvent(CSimpleFrame* frame, CSimpleEventType event, int32_t a4, int32_t priority)
#endif
{
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireStateMutation(true);
    Frame::Native::RequireSimpleFrame(frame);
    if (event > SIMPLE_EVENT_MOUSEWHEEL) throw std::invalid_argument("Invalid Frame event type");
    if (priority < -1) throw std::invalid_argument("Invalid Frame event priority");
    for (uint32_t i=0; i<m_eventqueue[event][frame->m_strata].m_count; ++i)
        if (m_eventqueue[event][frame->m_strata].m_array[i]->m_frame == frame)
            throw std::logic_error("Duplicate Frame event registration");
#endif
    CSimpleSortedArray<FRAMEPRIORITY*>& queue = this->m_eventqueue[event][frame->m_strata];

    if (priority == -1) {
        priority = frame->m_level;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    auto ownedPriority = std::make_unique<FRAMEPRIORITY>();
    FRAMEPRIORITY* framePriority = ownedPriority.get();
#else
    FRAMEPRIORITY* framePriority = static_cast<FRAMEPRIORITY*>(SMemAlloc(sizeof(FRAMEPRIORITY), __FILE__, __LINE__, 0));
#endif
    framePriority->m_frame = frame;
    framePriority->m_priority = priority;
    queue.Insert(framePriority);
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    ownedPriority.release();
#endif

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    // Reordering preserves capture but must refresh stationary hit priority.
    if (event == SIMPLE_EVENT_MOUSE) {
#else
    if (!a4 && event == SIMPLE_EVENT_MOUSE) {
#endif
        this->m_checkFocus = 1;
    }

}

// VERIFY: 4939f5d1 manual
// OFFSET: 0x00495900
#ifdef WOWDECOMP_PORTABLE_RUNTIME
void CSimpleTop::UnregisterForEvent(CSimpleFrame* frame, uint32_t event, int32_t a4)
#else
void CSimpleTop::UnregisterForEvent(CSimpleFrame* frame, CSimpleEventType event, int32_t a4)
#endif
{
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireStateMutation(true);
    Frame::Native::RequireSimpleFrame(frame);
    if (event > SIMPLE_EVENT_MOUSEWHEEL) throw std::invalid_argument("Invalid Frame event type");
    if (!a4 && event == SIMPLE_EVENT_MOUSE && frame == this->m_moveResizeFrame) this->StopMoveOrResizeFrame();
#endif
    CSimpleSortedArray<FRAMEPRIORITY*>& queue = this->m_eventqueue[event][frame->m_strata];

    for (uint32_t i = 0; i < queue.m_count; i++) {
        FRAMEPRIORITY* framePriority = queue.m_array[i];

        if (framePriority->m_frame != frame) {
            continue;
        }

        queue.Remove(i);
        delete framePriority;

        if (a4) {
            return;
        }

        if (event == SIMPLE_EVENT_KEY) {
            for (uint32_t key = 0; key < sizeof(this->m_keydownCapture) / sizeof(this->m_keydownCapture[0]); key++) {
                if (this->m_keydownCapture[key] == frame) {
                    this->m_keydownCapture[key] = nullptr;
                }
            }
        } else if (event == SIMPLE_EVENT_MOUSE) {
            if (frame == this->m_mouseCapture) {
                this->m_mouseCapture = nullptr;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
                frame->m_isMouseDown = frame->m_isMouseDragging = 0;
#endif
            }

            if (frame == this->m_mouseFocus) {
                this->m_mouseFocus = nullptr;
                this->m_checkFocus = 1;

                if (frame->m_lifecycleState != 3) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
                    Frame::Native::ResizeScope guard;
#endif
                    frame->OnLayerCursorExit(0, 1);
                }
            }
        }

        return;
    }

}

// VERIFY: f41311c7 auto
// OFFSET: 0x004959F0
void CSimpleTop::EnableEvents() {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableFrameFeature();
#else

    EventRegisterEx(EVENT_ID_CHAR, &CSimpleTop::OnChar, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_IME, &CSimpleTop::OnIme, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_KEYDOWN, &CSimpleTop::OnKeyDown, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_KEYUP, &CSimpleTop::OnKeyUp, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_KEYDOWN_REPEATING, &CSimpleTop::OnKeyDownRepeat, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_MOUSEMOVE, &CSimpleTop::OnMouseMove, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_MOUSEMOVE_RELATIVE, &CSimpleTop::OnMouseMoveRelative, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_MOUSEDOWN, &CSimpleTop::OnMouseDown, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_MOUSEUP, &CSimpleTop::OnMouseUp, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_MOUSEWHEEL, &CSimpleTop::OnMouseWheel, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_SIZE, &CSimpleTop::OnDisplaySizeChanged, this, EVENT_PRIORITY_ABOVE_NORMAL);
    EventRegisterEx(EVENT_ID_FOCUS, &CSimpleTop::OnFocusChanged, this, EVENT_PRIORITY_ABOVE_NORMAL);

#endif
}

// VERIFY: 8b4e6642 auto
// OFFSET: 0x00495B00
void CSimpleTop::DisableEvents() {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableFrameFeature();
#else

    EventUnregisterEx(EVENT_ID_CHAR, &CSimpleTop::OnChar, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_IME, &CSimpleTop::OnIme, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_KEYDOWN, &CSimpleTop::OnKeyDown, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_KEYUP, &CSimpleTop::OnKeyUp, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_KEYDOWN_REPEATING, &CSimpleTop::OnKeyDownRepeat, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_MOUSEMOVE, &CSimpleTop::OnMouseMove, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_MOUSEMOVE_RELATIVE, &CSimpleTop::OnMouseMoveRelative, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_MOUSEDOWN, &CSimpleTop::OnMouseDown, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_MOUSEUP, &CSimpleTop::OnMouseUp, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_MOUSEWHEEL, &CSimpleTop::OnMouseWheel, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_SIZE, &CSimpleTop::OnDisplaySizeChanged, this, 0xFFFFFFFF);
    EventUnregisterEx(EVENT_ID_FOCUS, &CSimpleTop::OnFocusChanged, this, 0xFFFFFFFF);

#endif
}

// INLINED
CFrameStrata::CFrameStrata() {
    FRAME_TREE_CHECK();
    this->m_batchDirty = 0;
    this->m_levelsDirty = 0;
    this->m_topLevel = 0;
}

// VERIFY: 46f7c2a5 manual
// OFFSET: 0x00495DE0
void CFrameStrata::AddFrame(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
    uint32_t level = frame->m_level;

    if (level >= this->m_levels.Count()) {
        uint32_t count = this->m_levels.Count();
        this->m_levels.SetCount(level + 1);

        for (; count <= level; count++) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->m_levels[count] = new CFrameStrataNode;
#else
            this->m_levels[count] = new (SMemAlloc(sizeof(CFrameStrataNode), __FILE__, __LINE__, 0)) CFrameStrataNode;
#endif
        }
    }

    if (level >= this->m_topLevel) {
        this->m_topLevel = level + 1;
    }

    CFrameStrataNode* node = this->m_levels[level];

    if (!frame->m_strataLink.IsLinked()) {
        TSExplicitList<CSimpleFrame, offsetof(CSimpleFrame, m_strataLink)>* list = node->m_pendingFrame ? &node->m_pendingFrames : &node->m_frames;
        list->LinkToTail(frame);

        if (!(frame->m_frameFlags & 0x2000)) {
            node->m_batchDirty = -1;
        }
    }

    this->m_levelsDirty = 1;
    this->m_batchDirty |= node->m_batchDirty != 0;
}

// VERIFY: a790458c manual
// OFFSET: 0x00495EB0
CSimpleTop::CSimpleTop() {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRenderer();
    if (CSimpleTop::s_instance) throw std::logic_error("Only one Frame root may be installed");
#endif
    this->m_mouseFocus = nullptr;
    this->m_mouseCapture = nullptr;
    this->m_checkFocus = 1;
    this->m_mouseButtonName = nullptr;
    this->m_mouseButtonCallback = nullptr;
    this->m_mouseMoveCallback = nullptr;
    this->m_displaySizeCallback = nullptr;
    this->m_focusChangedCallback = nullptr;
    this->m_keyDownCallback = nullptr;
    this->m_allowProtectedActions = 1;
    this->unused_1250 = 0;
    this->m_actionBlockedCallback = nullptr;

    CSimpleTop::s_instance = this;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->m_screenLayer = nullptr; // Native host owns scheduling and event delivery.
#else
    ScrnLayerCreate(nullptr, 1.0f, 4, nullptr, &PaintScreen, &this->m_screenLayer);
    this->EnableEvents();
#endif

    memset(this->m_keydownCapture, 0, sizeof(this->m_keydownCapture));

    NDCToDDC(1.0f, 1.0f, &this->m_rect.maxX, &this->m_rect.maxY);
    this->m_layoutFlags |= 0x100;

    this->m_moveResizeEnabled = 0;
    this->m_moveResizeFrame = nullptr;
    this->m_moveResizeReason = MOVERESIZE_REASON_NONE;
    this->m_moveResizePoint = FRAMEPOINT_TOPLEFT;
    this->m_moveResizeLast.x = 0.0f;
    this->m_moveResizeLast.y = 0.0f;

    for (int32_t strata = 0; strata < FRAME_STRATA_TOOLTIP + 1; strata++) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    this->m_strata[strata] = new CFrameStrata;
#else
        this->m_strata[strata] = new (SMemAlloc(sizeof(CFrameStrata), __FILE__, __LINE__, 0)) CFrameStrata;
#endif
    }

    this->m_mousePosition.m_mode = MOUSE_MODE_NORMAL;
    this->m_mousePosition.m_button = static_cast<MOUSEBUTTON>(0);
    this->m_mousePosition.m_buttonState = 0;
    this->m_mousePosition.m_metaKeyState = 0;
    this->m_mousePosition.m_flags = 0;
    this->m_mousePosition.m_x = 0.0f;
    this->m_mousePosition.m_y = 0.0f;
    this->m_mousePosition.m_wheelDistance = 0;
    this->m_mousePosition.m_time = 0;

    float x;
    float y;
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    x = y = 0.0f;
#else
    EventInputGetMousePosition(&x, &y);
#endif
    DDCToNDC(x, y, &this->m_mousePosition.m_x, &this->m_mousePosition.m_y);

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    // Renderer/font owners are installed before the root, and outlive it.
#else
    CSimpleRender::Init();
    CSimpleTexture::Init();
#endif
}

// VERIFY: 4c3b2719 auto
// OFFSET: 0x004960D0
void CSimpleTop::ShowFrame(CSimpleFrame* frame, int32_t a3) {
    FRAME_TREE_CHECK();
    this->m_strata[frame->m_strata]->AddFrame(frame);
    frame->RegisterForEvents(a3);
}

// VERIFY: 9bb3362f manual
// OFFSET: 0x00496100
CFrameStrata::~CFrameStrata() {
    FRAME_TREE_CHECK();
    for (uint32_t level = 0; level < this->m_levels.Count(); level++) {
        delete this->m_levels[level];
        this->m_levels[level] = nullptr;
    }
}

// VERIFY: caeade74 manual
// OFFSET: [0x00496170, 0x00496360]
CSimpleTop::~CSimpleTop() {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireTreeMutation();
#endif
    CSimpleFrame* frame;

    while ((frame = this->m_destroyed.Head()) != nullptr) {
        this->m_destroyed.UnlinkNode(frame);
    }

    while ((frame = this->m_frames.Head()) != nullptr) {
        this->m_frames.UnlinkNode(frame);
        delete frame;
    }

    for (int32_t strata = 0; strata < FRAME_STRATA_TOOLTIP + 1; strata++) {
        delete this->m_strata[strata];
        this->m_strata[strata] = nullptr;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::DrainDestroyQueue();
#else
    CSimpleFontString::FreeEmbededTextureStorage();
    CSimpleFont::Destroy();
    CSimpleTexture::Destroy();
    CSimpleRender::Destroy();

    this->DisableEvents();
    HandleClose(reinterpret_cast<HOBJECT>(this->m_screenLayer));
#endif
    CSimpleTop::s_instance = nullptr;
}

// VERIFY: a98d5c82 auto
// OFFSET: 0x004899F0
int32_t CSimpleTop::LowerFrame(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::UnavailableFrameFeature();
#else

    return 0;

#endif
}

// VERIFY: 0cc3f3ae auto
// OFFSET: 0x00632050
void CSimpleTop::ValidateDeletedFrame(CSimpleFrame* frame) {
    FRAME_TREE_CHECK();
}

#ifdef WOWDECOMP_PORTABLE_RUNTIME
void CSimpleTop::FlushMoveRaise() {
    Frame::Native::RequireTreeMutation();
    if (this->m_nativeMoveRaisePending) {
        this->m_nativeMoveRaisePending = false;
        if (this->m_moveResizeFrame) this->RaiseFrame(this->m_moveResizeFrame, 0);
    }
}
// Native host cancellation: clear owned state before any external callback.
void CSimpleTop::CancelMouseInput() {
    Frame::Native::RequireRoot(); Frame::Native::RequireTreeMutation();
    std::exception_ptr failure;
    try { this->StopMoveOrResizeFrame(); } catch (...) { failure = std::current_exception(); }
    Frame::Native::ResizeScope guard;
    auto* capture = m_mouseCapture; auto* focus = m_mouseFocus;
    m_mouseCapture = m_mouseFocus = nullptr;
    m_nativeButtonsDown = 0; m_nativeMouseSeen = false; m_checkFocus = 0;
    m_mousePosition.m_buttonState = 0;
    if (capture) {
        capture->m_isMouseDown = capture->m_isMouseDragging = 0;
        CMouseEvent event; event = m_mousePosition;
        event.m_data.m_button = 0; event.m_data.m_flags |= 1; event.m_id = 0x400500C9;
        try { capture->OnLayerMouseUp(event, nullptr); }
        catch (...) { failure = std::current_exception(); }
    }
    try { if (focus) focus->OnLayerCursorExit(0, 1); }
    catch (...) { if (!failure) failure = std::current_exception(); }
    if (failure) std::rethrow_exception(failure);
}
#endif
