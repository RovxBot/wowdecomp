#ifndef WOWDECOMP_NATIVE_GLUE_SCRIPT_SERVICES_H
#define WOWDECOMP_NATIVE_GLUE_SCRIPT_SERVICES_H
#include <cstdint>
namespace GlueNative {
// Call after the original CVar owner loads the external Config.wtf, before Glue.
// expansionLevel is the successfully mounted retail archive level, not account data.
void InitializeScriptServices(uint8_t expansionLevel);
}
#endif
