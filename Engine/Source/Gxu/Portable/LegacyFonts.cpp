#include "LegacyFonts.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Gxu/GxuFontFace.h"
#include <stdexcept>
#include <utility>
namespace Gxu { namespace Native {
namespace { LegacyFonts* active = nullptr; }
LegacyFonts::LegacyFonts(FontBytes bytes) : device(g_theGxDevicePtr), provider(std::move(bytes)) {
    if (!device || !provider || active)
        throw std::logic_error("Native Gxu requires one installed device and font provider");
    device->NativeCheckOwner();
    active = this;
    try { GxuFontInitialize(); }
    catch (...) { GxuFontShutdown(); active = nullptr; throw; }
}
LegacyFonts::~LegacyFonts() {
    try {
        Check();
        GxuFontShutdown();
        // External face references must not outlive the shared FreeType library.
        if (s_faceHash.m_fulllist.Head()) std::terminate();
        active = nullptr;
    } catch (...) { std::terminate(); }
}
void LegacyFonts::Check() const {
    if (g_theGxDevicePtr != device) throw std::logic_error("Native Gxu device lifetime ended");
    device->NativeCheckOwner();
}
std::vector<uint8_t> LegacyFonts::Read(const char* name) const {
    Check();
    auto bytes = provider(name);
    if (bytes.empty() || bytes.size() > 32 * 1024 * 1024u)
        throw std::out_of_range("Native Gxu font source must be 1..32 MiB");
    return bytes;
}
void RequireFontOwner() {
    if (!active) throw std::logic_error("Native Gxu font owner is not installed");
    active->Check();
}
std::vector<uint8_t> ReadFontBytes(const char* name) {
    RequireFontOwner();
    return active->Read(name);
}
}}
