#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Services/Portable/ShellSettings.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

using namespace Services::Native;
namespace File = Blizzard::File;
namespace fs = std::filesystem;
void Check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void Fails(const std::function<void()> &fn) {
    bool failed = false;
    try {
        fn();
    } catch (const std::exception &) {
        failed = true;
    }
    Check(failed, "Invalid settings unexpectedly succeeded");
}
int main() {
    try {
        uint64_t random;
        OsSecureRandom(reinterpret_cast<uint8_t*>(&random), sizeof(random));
        auto root = fs::temp_directory_path() / ("wowdecomp-shell-settings-" + std::to_string(random));
        fs::create_directories(root / "data");
        struct Cleanup {
            fs::path root;
            ~Cleanup() {
                std::error_code error;
                fs::remove_all(root, error);
            }
        } cleanup{root};
        File::Native::Paths paths;
        Check(File::Native::ConfigurePaths((root / "data").u8string().c_str(),
                                           (root / "settings").u8string().c_str(),
                                           (root / "cache").u8string().c_str(), paths),
              "Cannot configure fixture roots");
        auto first = LoadSettings(paths);
        Check(first.launches == 0 && first.text.empty(), "Absent settings did not use defaults");
        first.launches = 1;
        first.text = "fixture \xc3\xa9 \xe4\xb8\xad \xf0\x9f\x98\x80";
        SaveSettings(paths, first);
        Check(LoadSettings(paths) == first, "Settings UTF-8 save/reload failed");
        auto second = first;
        second.width = 800;
        second.height = 600;
        second.launches = 2;
        SaveSettings(paths, second);
        Check(LoadSettings(paths) == second, "Atomic settings replacement failed");
        auto invalid = second;
        invalid.text = "\xed\xa0\x80";
        Fails([&] { SaveSettings(paths, invalid); });
        Check(LoadSettings(paths) == second, "Rejected text destroyed prior settings");
        invalid = second;
        invalid.width = 0;
        Fails([&] { SaveSettings(paths, invalid); });
        Check(LoadSettings(paths) == second, "Rejected size destroyed prior settings");
        auto path = root / "settings/native-shell.settings", parked = root / "settings/previous.settings";
        Check(File::Move(path.u8string().c_str(), parked.u8string().c_str()), "Cannot park fixture settings");
        fs::create_directory(path);
        Fails([&] { SaveSettings(paths, first); });
        size_t entries = 0;
        for (const auto &item : fs::directory_iterator(root / "settings")) {
            ++entries;
            Check(item.path().extension() != ".tmp", "Failed settings save leaked a temporary file");
        }
        Check(entries == 2, "Failed replacement changed existing entries");
        fs::remove(path);
        Check(File::Move(parked.u8string().c_str(), path.u8string().c_str()),
              "Cannot restore fixture settings");
        Check(LoadSettings(paths) == second, "Failed replacement destroyed previous file");
        for (auto malformed :
             {"WOWDECOMP_NATIVE_SHELL 9\n", "WOWDECOMP_NATIVE_SHELL 1\nwidth 800\nwidth 800\n",
              "WOWDECOMP_NATIVE_SHELL 1\nwidth 800\nheight 600\nlaunches -1\ntext \n"}) {
            {
                std::ofstream file(path);
                file << malformed;
            }
            Fails([&] { LoadSettings(paths); });
        }
        Check(fs::is_empty(root / "data"), "Settings fixture wrote into data root");
        std::cout << "Native shell settings UTF-8, default/reload, atomic replacement, failure preservation "
                     "and cleanup passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
