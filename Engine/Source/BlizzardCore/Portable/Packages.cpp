#include "Package.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <filesystem>

namespace Blizzard { namespace Mopaq { namespace Native {
namespace {
// Original 12340 key, Client.cpp s_signatureModulus (0x00AB6028).
// This is public verification material, never a private signing key.
const Bytes modulus = {
    0x4F,0x6F,0xD4,0x65,0x56,0x85,0x88,0xD9,0xAA,0xE6,0xF7,0x05,0x72,0xBB,0x2A,0x20,
    0xE0,0x24,0x38,0x2B,0x77,0xB3,0x3D,0xC2,0xD3,0x2D,0x52,0x5C,0xB7,0xA3,0x0F,0xE6,
    0x0F,0x80,0x93,0x8D,0x5C,0xC4,0x81,0x5E,0x9D,0x58,0x8E,0xCA,0xA6,0x6F,0x27,0x36,
    0x52,0x58,0x0D,0xCE,0x65,0xE2,0xB5,0x7A,0xD2,0x39,0x74,0x8A,0xAC,0xAA,0xC1,0x0E,
    0xE5,0x67,0x2B,0x79,0xC1,0xFE,0x08,0x96,0x54,0x18,0x2A,0x34,0x31,0xA4,0x06,0x20,
    0x4B,0x12,0xD2,0x47,0x63,0x53,0x36,0x38,0x19,0x88,0x8E,0x50,0x2F,0x2F,0xD1,0x68,
    0xC8,0x5C,0x9D,0x3B,0xDE,0xFE,0x56,0x21,0xD1,0x59,0x4D,0x1C,0x32,0x1B,0x70,0x4B,
    0x40,0xE2,0x5E,0x0C,0x0E,0x4B,0x84,0xD5,0xD7,0x52,0x00,0x50,0x65,0x81,0xCE,0x17,
    0x53,0x25,0x49,0x9E,0x40,0xE9,0x04,0x94,0x2C,0xA7,0x93,0x5C,0x5D,0x54,0xC8,0xF9,
    0x6E,0xD0,0x69,0xFC,0xE5,0x8F,0xC0,0xB6,0x76,0xF2,0xEB,0x34,0x67,0x9F,0xF8,0xD0,
    0x34,0xB9,0xEA,0x9E,0x80,0xD0,0x29,0x8D,0xB4,0xB2,0xEB,0x3E,0xF4,0x4F,0xD0,0x84,
    0x43,0x15,0xF4,0x19,0xE3,0xC6,0x52,0xA2,0x18,0x40,0x08,0x07,0x21,0xF4,0x20,0xEC,
    0xE6,0x5A,0x9C,0x2B,0x08,0xF6,0x17,0x97,0x72,0xA6,0xD7,0xAB,0x64,0x0B,0x04,0x95,
    0x93,0x0D,0xCC,0x8F,0x02,0x30,0x91,0xA2,0xEE,0x30,0xE6,0x5F,0x2F,0xD4,0x13,0xB5,
    0x5D,0xCA,0x5D,0xCF,0x7D,0xC3,0x6F,0x6B,0x9D,0xBC,0x6D,0x86,0x4C,0x91,0xC5,0x51,
    0xE8,0xFD,0xA6,0x33,0x10,0xD1,0x12,0x8F,0x05,0x80,0xDA,0x42,0x57,0x0C,0xEB,0xC0,
};
}
Packages::Packages() : verificationModulus(modulus) {}
Packages::Packages(Bytes key) : verificationModulus(std::move(key)) {
    if (verificationModulus.size() != 256) throw Error("Package trust anchor must be 2048 bits");
}
void Packages::Mount(std::shared_ptr<Archive> archive, int priority, const std::string& prefix) {
    if (authenticated || !archive) throw Error("Cannot change authenticated package mounts");
    mounts.push_back({std::move(archive), priority, prefix.empty() ? "" : Name(prefix) + '\\'});
    std::stable_sort(mounts.begin(), mounts.end(), [](const auto& a, const auto& b) { return a.priority > b.priority; });
}
Loaded Packages::ReadRaw(const std::string& input, size_t limit) const {
    auto name = Name(input);
    for (const auto& mount : mounts) {
        bool deleted = false;
        std::string member = mount.prefix + name;
        auto bytes = mount.archive->Read(member, deleted, limit);
        if (deleted) throw Error("MPQ member deleted by patch: " + name);
        if (bytes) return {std::move(*bytes), mount.archive->Path(), member, false};
    }
    throw Error("MPQ member not found: " + name);
}
void Packages::Authenticate(const std::string& locale, bool expansion) {
    if (authenticated) throw Error("Package signatures already initialized");
    auto file = ReadRaw("signaturefile", 16 * 1024 * 1024);
    if (!VerifySignature(file.bytes, verificationModulus)) throw Error("Could not authenticate signaturefile");
    ReadSignatureRecords(file.bytes, locale, expansion, signatures);
    authenticated = true;
}
Loaded Packages::Read(const std::string& input, size_t limit) const {
    if (!authenticated) throw Error("Package reads require signaturefile authentication");
    auto name = Name(input);
    auto loaded = ReadRaw(name, limit);
    auto signature = signatures.find(name);
    if (signature != signatures.end()) {
        if (Digest(loaded.bytes) != signature->second) throw Error("MPQ file MD5 verification failed: " + name);
        loaded.signedFile = true;
    }
    return loaded;
}
std::vector<std::string> Packages::Mounts() const {
    std::vector<std::string> result;
    for (const auto& mount : mounts) result.push_back(std::to_string(mount.priority) + " " + mount.archive->Path() + " :: " + mount.prefix);
    return result;
}
std::shared_ptr<Packages> Packages::Open12340(const File::Native::Paths& paths, const std::string& locale) {
    static const std::vector<std::string> locales{"enUS","koKR","frFR","deDE","zhCN","zhTW","esES","esMX","ruRU"};
    if (std::find(locales.begin(), locales.end(), locale) == locales.end()) throw Error("Unsupported 12340 locale");
    auto result = std::make_shared<Packages>();
    auto open = [&](const std::string& name, int priority, bool required) -> std::shared_ptr<Archive> {
        std::string path;
        if (!File::Native::ResolveDataPath(paths, name.c_str(), path)) {
            if (SErrGetLastError() == 2 && !required) return nullptr;
            throw Error("Cannot resolve archive: " + name + " (error " + std::to_string(SErrGetLastError()) + ")");
        }
        auto archive = std::make_shared<Archive>(path);
        result->Mount(archive, priority); return archive;
    };
    // Same two-pass PatchFiles(false) ordering, expressed relative to the explicit
    // read-only Data root. No implicit cwd/parent/installation fallback.
    std::vector<std::string> patches;
    for (const auto& directory : {std::string(), locale + "/"}) {
        std::string host = paths.data;
        if (!directory.empty() && !File::Native::ResolveDataPath(paths, locale.c_str(), host)) throw Error("Locale directory unavailable");
        std::error_code error;
        std::filesystem::directory_iterator it(std::filesystem::u8path(host), error), end;
        if (error) throw Error("Cannot enumerate patch archives");
        auto pattern = directory.empty() ? "patch-?.MPQ" : "patch-" + locale + "-?.MPQ";
        for (; it != end; it.increment(error)) {
            if (error) throw Error("Cannot enumerate patch archives");
            auto name = it->path().filename().u8string();
            if (File::SimpleGlob(name.c_str(), pattern.c_str())) patches.push_back(directory + name);
        }
        if (error) throw Error("Cannot enumerate patch archives");
    }
    std::sort(patches.begin(), patches.end(), [](const auto& a, const auto& b) { return Name(a) > Name(b); });
    // pass 1 only appends existing patch.MPQ/patch-LOCALE.MPQ; optional absence is
    // distinguished from ambiguous paths and malformed archives by open().
    patches.push_back("patch.MPQ"); patches.push_back(locale + "/patch-" + locale + ".MPQ");
    std::vector<std::shared_ptr<Archive>> patchArchives;
    int priority = 64;
    for (auto it = patches.rbegin(); it != patches.rend(); ++it) {
        auto archive = open(*it, priority, false);
        if (archive) { patchArchives.push_back(archive); ++priority; }
    }
    std::vector<std::string> expansions;
    for (const auto& name : {"alternate", "expansion", "lichking"})
        if (open(std::string(name) + ".MPQ", priority, false)) { expansions.push_back(name); ++priority; }
    for (const auto& patch : patchArchives)
        for (const auto& expansion : expansions) result->Mount(patch, priority++, expansion);
    // Complete retail data (CheckDataFilesExist -> availability=2). Legacy split
    // trial/streaming data is unavailable explicitly, never silently emulated.
    open("common.MPQ", 49, true);
    open("common-2.MPQ", 48, false);
    open(locale + "/locale-" + locale + ".MPQ", 47, true);
    open(locale + "/speech-" + locale + ".MPQ", 46, true);
    open(locale + "/expansion-locale-" + locale + ".MPQ", 45, false);
    open(locale + "/lichking-locale-" + locale + ".MPQ", 44, false);
    open(locale + "/expansion-speech-" + locale + ".MPQ", 43, false);
    open(locale + "/lichking-speech-" + locale + ".MPQ", 42, false);
    result->Authenticate(locale, std::find(expansions.begin(), expansions.end(), "expansion") != expansions.end() ||
                                  std::find(expansions.begin(), expansions.end(), "lichking") != expansions.end());
    return result;
}
}}}
