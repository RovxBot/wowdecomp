#include "Engine/Source/BlizzardCore/Portable/Package.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>

using namespace Blizzard::Mopaq::Native;
namespace fs = std::filesystem;
void Check(bool value, const char* message) { if (!value) throw Error(message); }
Bytes Load(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw Error("Missing generated fixture");
    return Bytes(std::istreambuf_iterator<char>(file), {});
}
void Fails(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const Error&) { failed = true; }
    Check(failed, "Malformed/unsupported input unexpectedly succeeded");
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Pass generated fixture directory");
        fs::path root = fs::u8path(argv[1]);
        auto expected = Load(root / "expected.bin");
        auto base = std::make_shared<Archive>((root / "base.mpq").u8string());
        auto patch = std::make_shared<Archive>((root / "patch.mpq").u8string());
        bool deleted = false;
        for (auto name : {"raw.bin", "encrypted.bin", "sector.bin", "crc.bin", "single.bin"}) {
            auto value = base->Read(name, deleted);
            Check(value && *value == expected && !deleted, "MPQ encoding output differs");
        }
        Check(base->Read("EMPTY.txt", deleted)->empty(), "Empty/case lookup failed");
        Check(!base->Read("absent.bin", deleted) && !deleted, "Absent member found");
        Check(!patch->Read("gone.txt", deleted) && deleted, "Patch tombstone lost");
        Fails([&] { base->Read("badcrc.bin", deleted); });
        Fails([&] { base->Read("raw.bin", deleted, 4); });
        Archive v0((root / "v0.mpq").u8string());
        Check(v0.Read("PLAIN.txt", deleted) == std::optional<Bytes>(Bytes{'p','l','a','i','n','-','v'}), "Version 0 MPQ failed");
        for (auto method : {"zlib", "bzip2", "lzma"})
            Check(Decompress(Load(root / (std::string("codec-") + method + ".bin")), expected.size()) == expected, "Codec vector failed");
        for (auto method : {"sparse", "zlib-sparse", "bzip2-sparse"})
            Check(Decompress(Load(root / (std::string("codec-") + method + ".bin")), 100) == Bytes(100), "Sparse pipeline failed");
        Check(Decompress(Load(root / "codec-pkware.bin"), Load(root / "pkware-expected.bin").size(), true) == Load(root / "pkware-expected.bin"), "Bundled blast vector failed");
        Fails([] { Decompress(Bytes{0x20, 0, 0, 0, 100, 0xff}, 100); });
        Fails([] { Decompress(Bytes{0x20, 0, 0, 0, 100, 0}, 99); });
        Fails([] { Decompress(Bytes{1, 2, 3}, 100); });
        auto modulus = Load(root / "modulus.bin"), signature = Load(root / "signature.bin");
        Check(VerifySignature(signature, modulus), "Synthetic RSA known answer failed");
        for (size_t offset : {size_t(0), signature.size() - 260, signature.size() - 1}) {
            auto corrupt = signature; corrupt[offset] ^= 1;
            Check(!VerifySignature(corrupt, modulus), "Corrupt RSA signature accepted");
        }
        std::unordered_map<std::string, std::array<uint8_t,16>> records;
        ReadSignatureRecords(signature, "enUS", true, records);
        Check(records.size() == 4, "Locale/expansion signature selection failed");
        ReadSignatureRecords(signature, "frFR", false, records);
        Check(records.size() == 3 && records.count("LOCALE.TXT"), "Signature locale exclusion failed");
        Packages packages(modulus);
        packages.Mount(base, 1); packages.Mount(patch, 64); packages.Mount(patch, 65, "expansion");
        Fails([&] { packages.Read("raw.bin"); });
        packages.Authenticate("enUS", true);
        auto selected = packages.Read("shared.txt");
        Check(selected.bytes == Bytes({'v','i','e','w','-','v'}) && selected.signedFile &&
              selected.member == "EXPANSION\\SHARED.TXT", "Patch view/priority/digest failed");
        Check(packages.Read("locale.txt").signedFile, "Locale digest not enforced");
        Fails([&] { packages.Read("corrupt.txt"); });
        Fails([&] { packages.Read("gone.txt"); });
        Fails([&] { packages.Mount(base, 999); });
        Packages original;
        original.Mount(patch, 1);
        Fails([&] { original.Authenticate("enUS", true); }); // synthetic key never trusted by default
        for (auto name : {"../asset", "/asset", "C:\\asset", "a//b", "a/./b"}) Fails([&] { Name(name); });
        // Deterministic malformed codec corpus exercises bounded scalar paths.
        uint32_t random = 0x12345678;
        for (size_t i = 0; i < 1000; ++i) {
            Bytes input(1 + i % 64);
            for (auto& byte : input) { random = random * 1664525 + 1013904223; byte = static_cast<uint8_t>(random >> 24); }
            try { Decompress(input, 128); } catch (const Error&) {}
        }
        auto good = Load(root / "base.mpq");
        for (size_t offset : {size_t(0), size_t(4), size_t(12), size_t(14), size_t(24), size_t(28)}) {
            auto bad = good; bad[offset] ^= 0xff;
            auto path = root / "malformed.mpq";
            { std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(bad.data()), bad.size()); }
            Fails([&] { Archive invalid(path.u8string()); });
        }
        std::cout << "MPQ v0/v1, raw/encrypted/fixed-key/single/sector/Adler, codecs, patch views/tombstones, RSA/MD5/locale and malformed corpus passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
