#ifndef WOWDECOMP_NATIVE_GLUE_STARTUP_H
#define WOWDECOMP_NATIVE_GLUE_STARTUP_H
namespace GlueNative {
// Adapted CGlueMgr::Resume creation gate. No login state is reported until the
// complete ordered creation digest equals the authenticated original digest.
void LoadInterface();
}
#endif
