#ifndef WOWDECOMP_NATIVE_GLUE_SCRIPT_SERVICES_H
#define WOWDECOMP_NATIVE_GLUE_SCRIPT_SERVICES_H
namespace GlueNative {
// Call after the original CVar owner loads the external Config.wtf, before Glue.
void RegisterConsoleVariables();
}
#endif
