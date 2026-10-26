#ifndef WOWDECOMP_NATIVE_PACKAGE_H
#define WOWDECOMP_NATIVE_PACKAGE_H

#include "File.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Read-only native ABI package boundary. The legacy streaming/archive ABI is
// deliberately separate. No host loose-file override or downloader is installed.
namespace Blizzard { namespace Mopaq { namespace Native {
using Bytes = std::vector<uint8_t>;
class Error : public std::runtime_error { public: using std::runtime_error::runtime_error; };
std::string Name(const std::string& input);
// Exact 12340 codec set; malformed and unsupported streams fail visibly.
Bytes Decompress(const Bytes& source, size_t capacity, bool implode = false);
std::array<uint8_t, 16> Digest(const Bytes& data);

class Archive {
public:
    explicit Archive(const std::string& hostPath);
    ~Archive();
    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;
    // nullopt means absent; a tombstone is returned separately and stops lookup.
    std::optional<Bytes> Read(const std::string& name, bool& deleted,
                              size_t limit = 256 * 1024 * 1024) const;
    const std::string& Path() const;
private:
    struct State;
    std::unique_ptr<State> state;
};

struct Loaded { Bytes bytes; std::string archive; std::string member; bool signedFile = false; };
class Packages {
public:
    Packages();
    // Explicit trust anchor for embedders/redistributable fixture datasets.
    // Open12340 always selects the original key; the application exposes no
    // alternate-key or skip-verification option.
    explicit Packages(Bytes verificationModulus);
    // Reproduces Client::OpenArchives/PatchFiles for a complete retail 12340
    // installation. The data root is the installation's Data directory.
    static std::shared_ptr<Packages> Open12340(const File::Native::Paths& paths, const std::string& locale);
    void Mount(std::shared_ptr<Archive> archive, int priority, const std::string& prefix = "");
    // Required before ordinary reads. Uses the original 12340 public key,
    // signaturefile v2 RELEASE records and per-file MD5 policy.
    void Authenticate(const std::string& locale, bool expansion);
    Loaded Read(const std::string& name, size_t limit = 256 * 1024 * 1024) const;
    std::vector<std::string> Mounts() const;
    size_t SignatureCount() const { return signatures.size(); }
    // Original Client::OpenArchives installed expansion, independent of account
    // entitlements. Open12340 derives this from successfully mounted archives.
    uint8_t ExpansionLevel() const { return expansionLevel; }
private:
    struct MountPoint { std::shared_ptr<Archive> archive; int priority; std::string prefix; };
    Loaded ReadRaw(const std::string& name, size_t limit) const;
    std::vector<MountPoint> mounts;
    std::unordered_map<std::string, std::array<uint8_t, 16>> signatures;
    bool authenticated = false;
    uint8_t expansionLevel = 0;
    Bytes verificationModulus;
};

// Public-data RSA verifier shared with synthetic known-answer fixtures. Input
// and modulus use Storm's little-endian integer convention; exponent is 65537.
bool VerifySignature(const Bytes& file, const Bytes& modulus);
void ReadSignatureRecords(const Bytes& verifiedFile, const std::string& locale, bool expansion,
                         std::unordered_map<std::string, std::array<uint8_t, 16>>& records);
}}}
#endif
