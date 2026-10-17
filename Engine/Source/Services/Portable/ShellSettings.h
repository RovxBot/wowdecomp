#ifndef WOWDECOMP_NATIVE_SHELL_SETTINGS_H
#define WOWDECOMP_NATIVE_SHELL_SETTINGS_H
#include "Engine/Source/BlizzardCore/Portable/File.h"
#include <cstdint>

namespace Services {
namespace Native {
struct ShellSettings {
    int width = 1000, height = 700;
    uint64_t launches = 0;
    std::string text;
    bool operator==(const ShellSettings &other) const {
        return width == other.width && height == other.height && launches == other.launches &&
               text == other.text;
    }
};
// A separate native-shell settings file; the original WTF/config and game data
// are never edited by this diagnostic application.
ShellSettings LoadSettings(const Blizzard::File::Native::Paths &paths);
void SaveSettings(const Blizzard::File::Native::Paths &paths, const ShellSettings &settings);
} // namespace Native
} // namespace Services
#endif
