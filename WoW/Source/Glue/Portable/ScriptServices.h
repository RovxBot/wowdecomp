#ifndef WOWDECOMP_NATIVE_GLUE_SCRIPT_SERVICES_H
#define WOWDECOMP_NATIVE_GLUE_SCRIPT_SERVICES_H
#include <cstdint>
namespace GlueNative {
// Call after the original CVar owner loads the external Config.wtf, before Glue.
// expansionLevel is the successfully mounted retail archive level, not account data.
void InitializeScriptServices(uint8_t expansionLevel);
// Deliver pending Glue service failures from the outer Event idle phase, after
// interface activation and outside Lua/input callbacks. Unregistration cancels them.
void PollScriptServices();
}
#endif
