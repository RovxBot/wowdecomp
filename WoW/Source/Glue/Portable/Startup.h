#ifndef WOWDECOMP_NATIVE_GLUE_STARTUP_H
#define WOWDECOMP_NATIVE_GLUE_STARTUP_H
#include <cstdint>
namespace GlueNative {
// Original security-matrix templates request level 200 and create children.
// Bound their level domain separately from the unchanged 64-level parent graph.
constexpr uint32_t FrameLevelLimit = 512;
// Adapted CGlueMgr::Resume creation gate. No login state is reported until the
// complete ordered creation digest equals the authenticated original digest.
void LoadInterface();
}
#endif
