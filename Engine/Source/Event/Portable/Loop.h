#ifndef WOWDECOMP_NATIVE_EVENT_LOOP_H
#define WOWDECOMP_NATIVE_EVENT_LOOP_H
#include "Engine/Source/Event/Event.h"
namespace EventNative {
// Single original-client event context, driven on SDL's process main thread.
// No network scheduler is installed by this boundary.
void Dispatch(EVENTID, const void*);
void Tick(float elapsed, uint32_t time);
bool Closing();
}
#endif
