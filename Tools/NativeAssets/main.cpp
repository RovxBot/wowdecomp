#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace Blizzard;
namespace fs = std::filesystem;
std::string Hex(const std::array<uint8_t,16>& digest) {
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (auto byte : digest) out << std::setw(2) << unsigned(byte);
    return out.str();
}
int main(int argc, char** argv) {
    try {
        if (argc != 5) {
            std::cerr << "Usage: NativeAssetCheck DATA_DIRECTORY LOCALE SETTINGS_DIRECTORY CACHE_DIRECTORY\n";
            return 2;
        }
        File::Native::Paths paths;
        if (!File::Native::ConfigurePaths(argv[1], argv[3], argv[4], paths)) throw Mopaq::Native::Error("Cannot configure separate data/settings/cache roots");
        auto packages = Mopaq::Native::Packages::Open12340(paths, argv[2]);
        std::cout << "Authenticated original signaturefile: " << packages->SignatureCount() << " registered MD5 records\n";
        auto read = [&](const std::string& name) {
            auto loaded = packages->Read(name);
            std::cout << name << " | " << fs::u8path(loaded.archive).filename().u8string() << " :: " << loaded.member
                      << " | bytes=" << loaded.bytes.size() << " md5=" << Hex(Mopaq::Native::Digest(loaded.bytes))
                      << " registered-signature=" << (loaded.signedFile ? "verified" : "absent") << '\n';
            return loaded.bytes;
        };
        Assets::Dbc map(read("DBFilesClient\\Map.dbc"));
        std::cout << "DBC decoded rows=" << map.Rows() << " fields=" << map.Fields() << " first-id=" << map.UInt(0, 0) << '\n';
        auto images = Assets::DecodeBlp(read("Interface\\Glues\\LoadingBar\\Loading-BarFill.blp"));
        std::cout << "BLP decoded mipmaps=" << images.size() << " first=" << images[0].width << 'x' << images[0].height
                  << " rgba-md5=" << Hex(Mopaq::Native::Digest(images[0].rgba)) << '\n';
        Assets::Model model(read("Character\\Human\\Male\\HumanMale.m2"));
        auto vertex = model.Vertices()[0];
        std::cout << "M2 decoded name=" << model.Name() << " vertices=" << model.VertexCount() << " first="
                  << vertex.position.x << ',' << vertex.position.y << ',' << vertex.position.z << '\n';
        auto rootName = "World\\wmo\\Azeroth\\Buildings\\Duskwood_human_farm\\Duskwood_human_farm.wmo";
        auto wmo = Assets::DecodeWmo(read(rootName));
        std::cout << "WMO root decoded groups=" << wmo.groups << " materials=" << wmo.materials << '\n';
        auto group = Assets::DecodeWmo(read("World\\wmo\\Azeroth\\Buildings\\Duskwood_human_farm\\Duskwood_human_farm_000.wmo"));
        std::cout << "WMO group decoded vertices=" << group.vertices.size() << " triangles=" << group.indices.size()/3 << '\n';
        auto wdt = Assets::DecodeWdt(read("World\\Maps\\Azeroth\\Azeroth.wdt"));
        std::cout << "WDT decoded tiles=" << wdt.tiles.size() << " flags=" << wdt.flags << '\n';
        auto terrain = Assets::DecodeAdt(read("World\\Maps\\Azeroth\\Azeroth_32_48.adt"));
        std::cout << "ADT decoded cells=" << terrain.size() << " first-area=" << terrain[0].area
                  << " first-height=" << terrain[0].heights[0] << '\n';
        std::cout << "Representative 12340 decoding passed; game renderer/network/audio are not started\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "Asset inspection failed: " << error.what() << '\n'; return 1; }
}
