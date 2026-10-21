#ifndef WOWDECOMP_NATIVE_FRAME_WINDOW_EVENTS_H
#define WOWDECOMP_NATIVE_FRAME_WINDOW_EVENTS_H
#include "Engine/Source/Os/Input.h"
namespace Gx { namespace Native { class GlContext; }}
namespace Frame { namespace Native {
// Call after Window::Pump/OsQueueGet, before root update/render. The caller keeps
// unhandled events for its other consumers; this function never drains the queue.
// Only size delivery is selected. Query current logical/drawable dimensions rather
// than replay stale event dimensions. Zero-sized drawables wait for a later event.
// Root, device and context must be live on their owner, outside all traversals.
bool DispatchWindowEvent(Gx::Native::GlContext&, const OSEVENT&);
}}
#endif
