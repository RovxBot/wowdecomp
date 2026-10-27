#ifndef WOWDECOMP_NATIVE_CLIENT_LOOP_SCRIPT_ERRORS_H
#define WOWDECOMP_NATIVE_CLIENT_LOOP_SCRIPT_ERRORS_H
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include <stdexcept>
#include <string>

// The original protected Lua handler records failures without throwing through
// Lua. Check before paint so a later renderer failure cannot mask them.
inline void CheckLoopScriptErrors(uint64_t expected) {
    const auto actual = FrameScript::Native::ScriptErrorCount();
    if (actual != expected)
        throw std::runtime_error("Original Glue loop callback failed (" +
            std::to_string(actual - expected) + " protected errors; last): " +
            FrameScript::Native::LastScriptError());
}
#endif
