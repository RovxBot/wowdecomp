// Native adaptation of product 12340 Services/Texture.cpp file/cache ownership.
// This bounded consumer delivers original BLP2 mip chains through the qualified
// decoder and actual legacy callback/Gx objects. No donor ABI or loose fallback.
#include "Texture.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <thread>
#include <tuple>

namespace Services { namespace Native {
namespace {
std::atomic<TextureService*> active{nullptr};
constexpr size_t MaxTextures = 32, MaxPixels = 2 * 1024 * 1024, MaxSource = 32 * 1024 * 1024;
struct PixelBudget { std::atomic<size_t> bytes{0}; };
struct Key {
    std::string name;
    uint32_t filter, u, v;
    bool operator<(const Key& other) const {
        return std::tie(name, filter, u, v) < std::tie(other.name, other.filter, other.u, other.v);
    }
};
struct Image {
    Key key;
    CTexture* texture = nullptr; // Only the owner thread reads or changes this.
    Ticket ticket;
    std::vector<Blizzard::Assets::Image> levels; // Worker publishes before completion.
    std::shared_ptr<PixelBudget> budget;
    size_t heldBytes = 0;
    bool done = false;
    ~Image() { ClearPixels(); }
    void ClearPixels() {
        levels.clear();
        if (heldBytes) budget->bytes.fetch_sub(heldBytes);
        heldBytes = 0;
    }
    void Retain(std::vector<Blizzard::Assets::Image> pixels) {
        size_t bytes = 0;
        for (const auto& level : pixels) bytes += level.rgba.size();
        auto used = budget->bytes.load();
        do {
            if (bytes > 256 * 1024 * 1024 - used)
                throw std::out_of_range("Native image retained-pixel budget exceeded");
        } while (!budget->bytes.compare_exchange_weak(used, used + bytes));
        heldBytes = bytes;
        levels = std::move(pixels);
    }
    static void Source(EGxTexCommand command, uint32_t width, uint32_t height,
                       uint32_t face, uint32_t level, void* user, uint32_t& stride, const void*& bytes) {
        auto& image = *static_cast<Image*>(user);
        if (level >= image.levels.size() || face || width != image.levels[level].width ||
            height != image.levels[level].height)
            throw std::logic_error("Unavailable file-image callback extent/mip/face");
        if (command == GxTex_Latch) {
            stride = width * 4;
            bytes = image.levels[level].rgba.data();
        }
    }
};
TextureService& Owner() {
    auto* service = active.load();
    if (!service) throw std::logic_error("Native file image service is not installed");
    service->Check();
    return *service;
}
std::string Filename(const char* name) {
    if (!name || !*name) throw std::invalid_argument("Empty image filename");
    auto path = Blizzard::Mopaq::Native::Name(name);
    auto slash = path.find_last_of('\\'), dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) path += ".BLP";
    else if (path.substr(dot) != ".BLP") throw std::invalid_argument("Native file image service requires BLP2");
    if (path.size() >= 260) throw std::invalid_argument("Image filename exceeds original CTexture storage");
    return path;
}
}
struct TextureService::State {
    FileQueue& queue;
    Reader reader;
    CGxDevice* device;
    std::thread::id owner = std::this_thread::get_id();
    std::map<Key, std::shared_ptr<Image>> cache;
    std::map<CTexture*, std::shared_ptr<Image>> handles;
    std::vector<Ticket> requests;
    std::shared_ptr<PixelBudget> budget = std::make_shared<PixelBudget>();
    bool closing = false;
    State(FileQueue& q, Reader r) : queue(q), reader(std::move(r)), device(g_theGxDevicePtr) {}
    void Prune() {
        requests.erase(std::remove_if(requests.begin(), requests.end(), FileQueue::Complete), requests.end());
    }
};
TextureService::TextureService(FileQueue& queue, std::shared_ptr<const Blizzard::Mopaq::Native::Packages> packages)
    : TextureService(queue, [packages](const std::string& name, const Cancellation& cancel) {
        if (!packages) throw std::invalid_argument("Missing authenticated image packages");
        ReadResult result;
        if (cancel.Requested()) result.status = ReadStatus::Cancelled;
        else result.file = packages->Read(name, MaxSource);
        return result;
    }) {
    if (!packages) { active = nullptr; throw std::invalid_argument("Missing authenticated image packages"); }
}
TextureService::TextureService(FileQueue& queue, Reader reader) : state(new State(queue, std::move(reader))) {
    if (!state->device || !state->reader) throw std::invalid_argument("Images require an installed Gx device and reader");
    state->device->NativeCheckOwner();
    TextureService* empty = nullptr;
    if (!active.compare_exchange_strong(empty, this)) throw std::logic_error("Only one image service may be installed");
}
TextureService::~TextureService() {
    try {
        RequireIdle();
        state->closing = true;
        // Discarded handles can still have cooperative reads in progress. Finish
        // exactly one callback for each before freeing the captured service.
        for (const auto& request : state->requests) state->queue.Cancel(request);
        for (const auto& request : state->requests)
            if (!state->queue.Wait(request, std::chrono::seconds(30))) std::terminate();
        active = nullptr;
    } catch (...) { std::terminate(); }
}
void TextureService::Check() const {
    if (std::this_thread::get_id() != state->owner) throw std::logic_error("File images require their owner thread");
    if (g_theGxDevicePtr != state->device) throw std::logic_error("File images outlived the Gx device");
    state->device->NativeCheckOwner();
}
void TextureService::RequireIdle() const {
    Check();
    if (!state->handles.empty()) throw std::logic_error("Close file texture handles before image services");
}
size_t TextureService::Poll() { Check(); auto count = state->queue.Poll(); state->Prune(); return count; }
bool TextureService::Cancel(CTexture* texture) {
    Check();
    auto found = state->handles.find(texture);
    return found != state->handles.end() && state->queue.Cancel(found->second->ticket);
}
CTexture* CreateFileTexture(const char* filename, CGxTexFlags flags, CStatus* status, uint32_t options) {
    auto& service = Owner();
    auto& s = *service.state;
    if (s.closing) throw std::logic_error("Native image service is closing");
    // Explicit sampler (bit 0), exact BLP suffix (bit 3), UI priority (bit 4).
    // Disk lookup, atlas eligibility, streaming and other formats stay unavailable.
    if (!status || !(options & 1) || (options & ~0x19u) || flags.m_filter > GxTex_LinearMipLinear ||
        flags.m_forceMipTracking || flags.m_generateMipMaps || flags.m_renderTarget || flags.m_bit8 ||
        flags.m_maxAnisotropy != 1 || flags.m_bit14 || flags.m_bit15 || flags.m_unk16 || flags.m_bit31)
        throw std::invalid_argument("Unavailable native file texture options/sampler");
    Key key{Filename(filename), flags.m_filter, flags.m_wrapU, flags.m_wrapV};
    auto cached = s.cache.find(key);
    if (cached != s.cache.end()) {
        if (options & 0x10) s.queue.Promote(cached->second->ticket, 129);
        HandleDuplicate(reinterpret_cast<HOBJECT>(cached->second->texture));
        return cached->second->texture;
    }
    s.Prune();
    if (s.handles.size() >= MaxTextures || s.requests.size() >= MaxTextures)
        throw std::out_of_range("Native image service capacity exceeded");
    auto image = std::make_shared<Image>();
    image->key = key;
    image->budget = s.budget;
    auto texture = std::make_unique<CTexture>();
    texture->m_gxTexFlags = flags;
    SStrCopy(texture->m_filename, key.name.c_str(), sizeof(texture->m_filename));
    image->texture = texture.get();
    s.cache.emplace(key, image);
    try { s.handles.emplace(texture.get(), image); }
    catch (...) { s.cache.erase(key); throw; }
    try {
        // Reserve before submission: a failed allocation must not strand an
        // untracked callback capturing the service or a partially built handle.
        s.requests.reserve(s.requests.size() + 1);
        auto reader = s.reader;
        image->ticket = s.queue.Submit([reader, image](const Cancellation& cancel) {
            auto result = reader(image->key.name, cancel);
            if (result.status != ReadStatus::Success || cancel.Requested()) return result;
            const auto& bytes = result.file.bytes;
            if (bytes.size() < 148 || bytes.size() > MaxSource) throw std::out_of_range("File image source extent");
            auto u32 = [&](size_t o) { return uint32_t(bytes[o]) | uint32_t(bytes[o+1]) << 8 |
                uint32_t(bytes[o+2]) << 16 | uint32_t(bytes[o+3]) << 24; };
            auto width = u32(12), height = u32(16);
            if (width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1)) ||
                uint64_t(width) * height > MaxPixels) throw std::out_of_range("File image dimensions/pixel budget");
            auto levels = Blizzard::Assets::DecodeBlp(bytes);
            if (image->key.filter >= GxTex_NearestMipNearest) {
                size_t count = 1;
                for (auto extent = std::max(width, height); extent > 1; extent >>= 1) ++count;
                if (levels.size() != count) throw std::invalid_argument("Mipped file texture requires a complete authored chain");
            } else levels.resize(1); // Decode still validates every supplied level.
            image->Retain(std::move(levels));
            result.file.bytes.clear();
            result.file.bytes.shrink_to_fit();
            return result;
        }, [image](const ReadResult& result) {
            if (!image->texture) return; // Last handle closed; callback still delivered once.
            auto* texture = image->texture;
            image->done = true;
            if (result.status != ReadStatus::Success) {
                image->ClearPixels();
                texture->m_status.Add(STATUS_ERROR, "Image %s: %s", texture->m_filename,
                    result.status == ReadStatus::Cancelled ? "read cancelled" : result.error.c_str());
                return;
            }
            try {
                texture->m_gxWidth = image->levels.front().width;
                texture->m_gxHeight = image->levels.front().height;
                texture->m_dataFormat = texture->m_gxTexFormat = GxTex_Abgr8888;
                bool opaque = true;
                for (const auto& level : image->levels)
                    for (size_t i = 3; i < level.rgba.size(); i += 4)
                        if (level.rgba[i] != 255) { opaque = false; break; }
                texture->m_flags = opaque ? 1 : 0;
                texture->m_alphaBits = opaque ? 0 : 8;
                if (!GxTexCreate(GxTex_2d, texture->m_gxWidth, texture->m_gxHeight, 0,
                    GxTex_Abgr8888, GxTex_Abgr8888, texture->m_gxTexFlags, image.get(), Image::Source,
                    texture->m_filename, texture->m_gxTex)) throw std::runtime_error("Gx allocation failed");
                GxTexUpdate(texture->m_gxTex, 0, 0, texture->m_gxWidth, texture->m_gxHeight, 1);
            } catch (const std::exception& error) {
                if (texture->m_gxTex) { GxTexDestroy(texture->m_gxTex); texture->m_gxTex = nullptr; }
                texture->m_status.Add(STATUS_ERROR, "Image %s: %s", texture->m_filename, error.what());
                image->ClearPixels();
            }
        }, options & 0x10 ? 129 : 130);
        s.requests.push_back(image->ticket);
    } catch (...) {
        s.handles.erase(texture.get()); s.cache.erase(key); throw;
    }
    HandleCreate(texture.get(), "HTEXTURE");
    return texture.release();
}
void ReleaseFileTexture(CTexture* texture) {
    auto* service = active.load();
    if (!service) return;
    service->Check();
    auto& s = *service->state;
    auto found = s.handles.find(texture);
    if (found == s.handles.end()) return; // Callback-only Gxu atlas.
    auto image = found->second;
    image->texture = nullptr;
    s.queue.Cancel(image->ticket);
    s.cache.erase(image->key);
    s.handles.erase(found);
    // CTexture already dropped the Gx object/bindings before releasing pixels.
}
CGxTex* GetFileTexture(CTexture* texture, int32_t wait, CStatus* status) {
    if (!texture) return nullptr;
    texture->NativeCheckOwner();
    // Original wait 2 only avoids blocking for remote streaming (m_char24).
    // Native retained readers deliver local data; streaming remains unavailable.
    if (wait < 0 || wait > 2) throw std::invalid_argument("Native file texture wait must be 0, 1 or 2");
    auto* service = active.load();
    if (service) {
        service->Check();
        auto found = service->state->handles.find(texture);
        if (found != service->state->handles.end()) {
            auto image = found->second;
            if (!image->done && wait && !service->state->queue.Wait(image->ticket, std::chrono::seconds(30)))
                throw std::runtime_error("Native file texture wait timed out");
            if (status && image->done && status != &texture->m_status) status->Add(texture->m_status);
        }
    }
    return texture->m_gxTex;
}
}}

CTexture* TextureCreate(const char* filename, CGxTexFlags flags, CStatus* status, uint32_t options) {
    return Services::Native::CreateFileTexture(filename, flags, status, options);
}
int32_t TextureGetDimensions(CTexture* texture, uint32_t* width, uint32_t* height, int32_t wait) {
    if (!Services::Native::GetFileTexture(texture, wait, nullptr)) return 0;
    if (width) *width = texture->m_gxWidth;
    if (height) *height = texture->m_gxHeight;
    return 1;
}
int32_t TextureIsLoaded(CTexture* texture) { return Services::Native::GetFileTexture(texture, 0, nullptr) != nullptr; }
int32_t TextureIsReadyNow(CTexture* texture) { return TextureIsLoaded(texture); }
int32_t TextureIsOpaque(CTexture* texture) {
    return TextureIsLoaded(texture) && (texture->m_flags & 1);
}
const char* TextureGetFilename(CTexture* texture) {
    if (!texture) return nullptr;
    texture->NativeCheckOwner(); return texture->m_filename;
}
void TextureGetGxTexFlags(CTexture* texture, CGxTexFlags* flags) {
    if (!texture || !flags) throw std::invalid_argument("Missing texture/flags");
    texture->NativeCheckOwner(); *flags = texture->m_gxTexFlags;
}
