#ifndef WOWDECOMP_NATIVE_STARTUP_LOCALE_H
#define WOWDECOMP_NATIVE_STARTUP_LOCALE_H
#include <clocale>
#include <cstring>
#include <stdexcept>
// Run before SDL/workers. Keep the original C numeric locale for Lua/serialization.
inline void InitializeNativeCharacterLocale() {
    const char* selected = std::setlocale(LC_CTYPE, "");
    if (!selected || !std::strcmp(selected, "C") || !std::strcmp(selected, "POSIX"))
        selected = std::setlocale(LC_CTYPE, "C.UTF-8");
    if (!selected) throw std::runtime_error("Native client requires a UTF-8 character locale");
}
#endif
