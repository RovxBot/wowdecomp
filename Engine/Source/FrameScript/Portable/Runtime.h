#ifndef WOWDECOMP_NATIVE_FRAMESCRIPT_RUNTIME_H
#define WOWDECOMP_NATIVE_FRAMESCRIPT_RUNTIME_H
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace FrameScript { namespace Native {
enum class SourceKind { Interface, SavedVariables };
// Return completed reads, delivered on the owner thread by the retained file
// worker. Interface bytes must come from authenticated packages (and later the
// separate TOC signature traversal); saved variables use an external user root.
// No native ticket is a legacy SFile/CAsyncObject handle. False is a read error;
// an empty successful read is a valid Lua chunk.
using SourceReader = std::function<bool(SourceKind, const std::string&, std::vector<uint8_t>&)>;
class Runtime final {
public:
    explicit Runtime(SourceReader reader, bool profile = false);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    void Check() const;
    bool Read(SourceKind, const char*, std::string& normalized, std::vector<uint8_t>&) const;
private:
    std::thread::id owner;
    SourceReader reader;
};
void RequireOwner();
void RequireIdle();
void ObjectCreated();
void ObjectDestroyed();
bool ReadSource(SourceKind, const char*, std::string&, std::vector<uint8_t>&);
// Prevent Flush/Destroy from closing an executing VM, including nested C calls.
struct Execution final {
    Execution();
    ~Execution();
    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;
};
}}
#endif
