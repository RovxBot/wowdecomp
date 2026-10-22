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
// Committed Unicode text, keys, pointer move/buttons/wheel/relative, capture loss,
// focus and UTF-8 IME. Pointer coordinates use logical pixels, not drawable size.
// Dispatch IME immediately after OsQueueGet, before any further dequeue; OsIME
// getters borrow that event's snapshot. Consumers must not pump/drain the queue.
// Language-mode selection (IME action zero) remains unsupported by SDL.
// Unsupported events remain with the host. Unhandled selected events may also
// propagate to other consumers. This does not install Lua/XML input factories.
enum class InputResult { Unsupported, Unhandled, Handled };
InputResult DispatchInputEvent(Gx::Native::GlContext&, const OSEVENT&);
}}
#endif
