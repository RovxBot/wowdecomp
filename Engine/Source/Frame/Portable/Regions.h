#ifndef WOWDECOMP_NATIVE_FRAME_REGIONS_H
#define WOWDECOMP_NATIVE_FRAME_REGIONS_H
#include "Fonts.h"
#include "Render.h"
#include <stdexcept>
namespace Frame { namespace Native {
// Direct, unparented C++ region consumers precede original Frame/root factories.
// Public fields stay borrowed owner-thread state; batches retain lower handles.
inline void RequireRegions() {
    RequireFontFactories();
    RequireRenderIdle();
}
[[noreturn]] inline void UnavailableRegionFeature() {
    throw std::logic_error("Native Frame parents/root, animation, XML/script and embedded textures are not installed");
}
}}
#endif
