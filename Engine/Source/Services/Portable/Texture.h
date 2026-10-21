#ifndef WOWDECOMP_NATIVE_IMAGE_SERVICES_H
#define WOWDECOMP_NATIVE_IMAGE_SERVICES_H
#include "AsyncFile.h"
#include <memory>
class CTexture;
class CGxTex;
class CStatus;
class CGxTexFlags;

namespace Services { namespace Native {
// File-backed original CTexture/HTEXTURE handles on the installed Gx owner.
// Production reads use authenticated Packages; the explicit reader overload is
// for embedders/fixtures. Readers execute on the retained file worker and must
// deliver local data; remote streaming has no native wait-2 contract yet.
// Close all file texture handles, then this service, then device/context/queue.
// Native tickets never occupy CTexture::m_asyncObject or impersonate CAsyncObject.
class TextureService final {
public:
    using Reader = std::function<ReadResult(const std::string&, const Cancellation&)>;
    // Optional read-only loose-image root. Option 2 checks it before packages;
    // without a mounted root, that eligibility still resolves through packages.
    TextureService(FileQueue&, std::shared_ptr<const Blizzard::Mopaq::Native::Packages>, std::string diskRoot = {});
    TextureService(FileQueue&, Reader, std::string diskRoot = {});
    ~TextureService();
    TextureService(const TextureService&) = delete;
    TextureService& operator=(const TextureService&) = delete;
    void Check() const;
    void RequireIdle() const;
    size_t Poll();
    bool Cancel(CTexture*);
    void EnableAtlas(bool);
    void SetFilter(uint32_t);
    struct State;
private:
    std::unique_ptr<State> state;
    friend CTexture* CreateFileTexture(const char*, CGxTexFlags, CStatus*, uint32_t);
    friend void ReleaseFileTexture(CTexture*);
    friend CGxTex* GetFileTexture(CTexture*, int32_t, CStatus*);
};
CTexture* CreateFileTexture(const char*, CGxTexFlags, CStatus*, uint32_t);
void ReleaseFileTexture(CTexture*);
CGxTex* GetFileTexture(CTexture*, int32_t wait, CStatus*);
}}
#endif
