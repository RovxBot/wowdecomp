#ifndef WOWDECOMP_NATIVE_LEGACY_FILES_H
#define WOWDECOMP_NATIVE_LEGACY_FILES_H
#include "AsyncFile.h"

namespace Services { namespace Native {
// Bounded original SFile/CAsyncObject adapter for local model/skin/animation
// callers. Native tickets remain private; workers never touch legacy objects,
// caller buffers or callbacks. Poll delivers legacy callbacks outside FileQueue's
// pump, so a model completion can open/enqueue its skin without recursive pumping.
// Close model owners and their objects/handles before this adapter, then queue.
class LegacyFiles final {
public:
    using Reader = std::function<ReadResult(const std::string&, const Cancellation&)>;
    LegacyFiles(FileQueue&, std::shared_ptr<const Blizzard::Mopaq::Native::Packages>);
    LegacyFiles(FileQueue&, Reader);
    ~LegacyFiles();
    LegacyFiles(const LegacyFiles&) = delete;
    LegacyFiles& operator=(const LegacyFiles&) = delete;
    void Poll();
    void CheckCanClose() const;
    struct State;
private:
    std::unique_ptr<State> state;
};
}}
#endif
