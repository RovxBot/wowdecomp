#ifndef WOWDECOMP_NATIVE_FRAME_REGIONS_H
#define WOWDECOMP_NATIVE_FRAME_REGIONS_H
#include "Fonts.h"
#include "Render.h"
#include <stdexcept>
class CSimpleFrame;
class CScriptRegion;
namespace Frame { namespace Native {
void RequireSimpleFrame(CSimpleFrame*);
void RequireParent(CScriptRegion*, CSimpleFrame*);
void RequireTreeMutation();
void RequireStateMutation();
// Direct C++ region consumers; Frame roots and parents are optional until created.
// Public fields stay borrowed owner-thread state; batches retain lower handles.
inline void RequireRegions() {
    RequireFontFactories();
    RequireRenderIdle();
}
[[noreturn]] inline void UnavailableRegionFeature() {
    throw std::logic_error("Native Frame animation, XML/script and embedded textures are not installed");
}
}}
#endif
