// Asset-free behavioral checks for the new full-client policy boundary.
#include "RovNativePolicy.h"
#include <fstream>
#include <unistd.h>

static void Check(bool condition) { if (!condition) std::abort(); }
int main() {
    namespace fs = std::filesystem;
    char name[] = "/tmp/wow-policy-XXXXXX";
    Check(mkdtemp(name));
    fs::path root(name), game = root / "original", state = root / "state";
    fs::create_directories(game / "Data"); fs::create_directories(state / "WTF");
    fs::create_directories(root / "cache"); fs::create_directories(root / "logs");
    fs::create_directory_symlink(game / "Data", state / "Data");
    setenv("WOWDECOMP_GAME_ROOT", game.c_str(), 1);
    setenv("WOWDECOMP_SETTINGS", state.c_str(), 1);
    setenv("WOWDECOMP_CACHE", (root / "cache").c_str(), 1);
    setenv("WOWDECOMP_LOGS", (root / "logs").c_str(), 1);
    Check(RovNative::AllowedWrite((state / "WTF/Config.wtf").c_str()));
    Check(RovNative::AllowedWrite((root / "cache/x").c_str()));
    Check(!RovNative::AllowedWrite((game / "Data/new.MPQ").c_str()));
    Check(!RovNative::AllowedWrite((state / "Data/new.MPQ").c_str()));
    Check(!RovNative::AllowedWrite((state / "../outside").c_str()));
    fs::create_directory_symlink(game, state / "WTF/escape");
    Check(!RovNative::AllowedWrite((state / "WTF/escape/new").c_str()));
    Check(!RovNative::TestServerEnabled());
    setenv("WOWDECOMP_ISOLATED_SERVER_LOGIN", "1", 1);
    Check(RovNative::TestServerEnabled() == bool(WOWDECOMP_ISOLATED_SERVER_BUILD));
    setenv("WOWDECOMP_ISOLATED_SERVER_LOGIN", "true", 1);
    Check(!RovNative::TestServerEnabled());
    fs::remove_all(root);
    std::puts("full-linux-policy=passed: normal credential refusal, explicit test gate, resolved write boundaries");
}
