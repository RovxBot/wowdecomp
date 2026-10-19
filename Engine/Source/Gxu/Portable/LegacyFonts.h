#ifndef WOWDECOMP_NATIVE_LEGACY_FONTS_H
#define WOWDECOMP_NATIVE_LEGACY_FONTS_H
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
class CGxDevice;
namespace Gxu { namespace Native {
// Owns the original Gxu globals on the installed device's main/context thread.
// Providers return completed, authenticated reads; no legacy SFile/ticket casts.
// Destroy strings/batches and close external HFACE handles before this owner,
// and destroy this owner before the device/context. Only one may be installed.
class LegacyFonts final {
public:
    using FontBytes = std::function<std::vector<uint8_t>(const std::string&)>;
    explicit LegacyFonts(FontBytes);
    ~LegacyFonts();
    LegacyFonts(const LegacyFonts&) = delete;
    LegacyFonts& operator=(const LegacyFonts&) = delete;
    void Check() const;
    std::vector<uint8_t> Read(const char*) const;
private:
    CGxDevice* device;
    FontBytes provider;
};
void RequireFontOwner();
std::vector<uint8_t> ReadFontBytes(const char*);
}}
#endif
