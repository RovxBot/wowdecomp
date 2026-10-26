// Native adaptation of product 12340 Services/Texture.cpp file/cache ownership.
// This bounded consumer delivers original BLP2 mip chains through the qualified
// decoder and actual legacy callback/Gx objects. No donor ABI or loose fallback.
#include "Texture.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <filesystem>
#include <thread>
#include <tuple>

namespace Services { namespace Native {
namespace {
std::atomic<TextureService*> active{nullptr};
// Signed GlueXML eagerly creates images for hidden screens as well as login.
// Bound metadata independently of the shared 256 MiB retained-pixel budget.
constexpr size_t MaxTextures = 1024, MaxPixels = 2 * 1024 * 1024, MaxSource = 32 * 1024 * 1024;
struct PixelBudget { std::atomic<size_t> bytes{0}; };
struct Key {
    std::string name;
    uint32_t filter, u, v;
    bool disk, atlas, defaultFilter;
    bool operator<(const Key& other) const {
        return std::tie(name, filter, u, v, disk, atlas, defaultFilter) <
            std::tie(other.name, other.filter, other.u, other.v, other.disk, other.atlas, other.defaultFilter);
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
    bool atlasSafe = false;
    bool smallDefault = false;
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
// Original atlas coordinates and slot accounting, with retained whole-sheet
// callback pixels. This avoids exporting bit15's legacy partial-source ABI.
struct Atlas {
    Image pixels;
    CTextureAtlas legacy;
    Atlas(uint32_t side, CGxTexFlags flags, std::shared_ptr<PixelBudget> budget) {
        legacy.m_width = legacy.m_height = side;
        legacy.m_capacity = 64;
        legacy.m_format = legacy.m_dataFormat = GxTex_Abgr8888;
        legacy.m_texFlags = flags;
        pixels.budget = std::move(budget);
        pixels.Retain({{side * 8, side * 8, std::vector<uint8_t>(side * side * 64 * 4)}});
        if (!GxTexCreate(GxTex_2d, side * 8, side * 8, 0, GxTex_Abgr8888, GxTex_Abgr8888,
            flags, &pixels, Image::Source, "Native image atlas", legacy.m_gxTex))
            throw std::runtime_error("Atlas Gx allocation failed");
        GxTexUpdate(legacy.m_gxTex, 0, 0, side * 8, side * 8, 1);
    }
    ~Atlas() {
        // GPU callbacks/bindings must retire before retained pixels.
        if (legacy.m_gxTex) { GxTexDestroy(legacy.m_gxTex); legacy.m_gxTex = nullptr; }
    }
    void Copy(uint32_t slot, const uint8_t* source) {
        const auto side = legacy.m_width, stride = side * 8 * 4;
        const auto x = (slot & 7) * side, y = (slot >> 3) * side;
        auto& bytes = pixels.levels.front().rgba;
        for (uint32_t row = 0; row < side; ++row) {
            auto* dest = bytes.data() + (y + row) * stride + x * 4;
            if (source) std::copy_n(source + row * side * 4, side * 4, dest);
            else std::fill_n(dest, side * 4, 0);
        }
        // Original CTextureAtlas::Update uploads immediately. The legacy dirty
        // rectangle is replaced by each mark, so deferring several completions
        // until the next draw would lose all but the last cell's update.
        GxTexUpdate(legacy.m_gxTex, x, y, x + side, y + side, 1);
    }
};

ReadResult ReadImage(const TextureService::Reader& reader, const std::string& root,
                     const Key& key, const Cancellation& cancel) {
    if (cancel.Requested()) { ReadResult r; r.status = ReadStatus::Cancelled; return r; }
    if (key.disk && !root.empty()) {
        namespace File = Blizzard::File;
        std::string path;
        if (File::Native::ResolveDataPath({root, {}, {}}, key.name.c_str(), path)) {
            File::StreamRecord* stream = nullptr;
            if (!File::Native::Open(path.c_str(), 1 | 4, File::Native::Disposition::OpenExisting, stream))
                throw std::runtime_error("Loose image open failed");
            struct Close { File::StreamRecord* file; ~Close() { File::Close(file); } } close{stream};
            File::FileInfo info{};
            if (!File::GetFileInfo(stream, &info) || !info.m_regularFile || info.m_size < 0 ||
                uint64_t(info.m_size) > MaxSource) throw std::out_of_range("Loose image source extent/type");
            ReadResult result;
            result.file.archive = path; result.file.member = key.name;
            result.file.bytes.resize(size_t(info.m_size));
            int32_t count = int32_t(info.m_size);
            if (!File::Read(stream, result.file.bytes.data(), &count) || count != info.m_size)
                throw std::runtime_error("Loose image short/failed read");
            return result;
        }
        const auto error = SErrGetLastError();
        // Only absence permits package lookup. Ambiguity, root escape and I/O
        // failures must never select a different source behind the caller's back.
        if (error != 2 && error != 3) throw std::runtime_error("Loose image path lookup failed");
    }
    return reader(key.name, cancel);
}
TextureService& Owner() {
    auto* service = active.load();
    if (!service) throw std::logic_error("Native file image service is not installed");
    service->Check();
    return *service;
}
std::string Filename(const char* name, bool exact = false) {
    if (!name || !*name) throw std::invalid_argument("Empty image filename");
    auto path = Blizzard::Mopaq::Native::Name(name);
    auto slash = path.find_last_of('\\'), dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) path += ".BLP";
    // Original TextureCreate tries the BLP basename first unless bit 3 asks
    // for an exact suffix. Original GlueXML also names these members as .tga.
    else if (!exact && path.substr(dot) == ".TGA") path.replace(dot, std::string::npos, ".BLP");
    else if (path.substr(dot) != ".BLP")
        throw std::invalid_argument("Native image decoder requires BLP2; unsupported source: " + path);
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
    std::string diskRoot;
    std::vector<std::unique_ptr<Atlas>> atlases;
    bool atlasEnabled = false;
    uint32_t filter = GxTex_LinearMipNearest;
    bool closing = false;
    State(FileQueue& q, Reader r, std::string root) : queue(q), reader(std::move(r)), device(g_theGxDevicePtr), diskRoot(std::move(root)) {}
    void Attach(Image& image) {
        auto* texture = image.texture;
        auto flags = texture->m_gxTexFlags;
        const auto side = texture->m_gxWidth;
        // Atlas eligibility is advisory. Preserve standalone repeat/mip behavior
        // when this bounded single-level, clamped sheet cannot represent it.
        if (!image.key.atlas || !image.atlasSafe || side != texture->m_gxHeight || side > 64 ||
            flags.m_filter > GxTex_Linear || flags.m_wrapU || flags.m_wrapV) return;
        Atlas* best = nullptr;
        for (auto& atlas : atlases)
            if (atlas->legacy.m_width == side && atlas->legacy.m_texFlags == flags &&
                atlas->legacy.m_used < 64 && (!best || atlas->legacy.m_used > best->legacy.m_used)) best = atlas.get();
        if (!best) {
            auto atlas = std::make_unique<Atlas>(side, flags, budget);
            best = atlas.get(); atlases.push_back(std::move(atlas));
        }
        auto& a = best->legacy;
        uint32_t slot = a.m_freeSlot;
        if (slot == uint32_t(-1)) {
            slot = 0;
            while (slot < 64 && a.m_textures[slot]) ++slot;
        }
        if (slot >= 64) throw std::logic_error("Atlas slot accounting failed");
        best->Copy(slot, image.levels.front().rgba.data());
        a.m_freeSlot = uint32_t(-1);
        a.m_textures[slot] = texture; ++a.m_used; ++a.m_count;
        texture->m_atlas = &a; texture->m_atlasBlockIndex = int32_t(slot);
        texture->m_flags |= 4;
        image.ClearPixels(); // The sheet now owns the retained copy.
    }
    void Detach(CTexture* texture) {
        if (!texture->m_atlas) return;
        auto found = std::find_if(atlases.begin(), atlases.end(), [&](const std::unique_ptr<Atlas>& a) {
            return &a->legacy == texture->m_atlas;
        });
        if (found == atlases.end()) throw std::logic_error("Foreign native image atlas");
        auto& a = (*found)->legacy;
        const auto slot = texture->m_atlasBlockIndex;
        if (slot < 0 || slot >= 64 || a.m_textures[slot] != texture) throw std::logic_error("Foreign atlas slot");
        if (a.m_used > 1) (*found)->Copy(uint32_t(slot), nullptr);
        a.m_textures[slot] = nullptr; --a.m_used; --a.m_count; a.m_freeSlot = uint32_t(slot);
        texture->m_atlas = nullptr; texture->m_atlasBlockIndex = -1;
        if (!a.m_used) atlases.erase(found);
    }
    void Prune() {
        requests.erase(std::remove_if(requests.begin(), requests.end(), FileQueue::Complete), requests.end());
    }
};
TextureService::TextureService(FileQueue& queue, std::shared_ptr<const Blizzard::Mopaq::Native::Packages> packages, std::string root)
    : TextureService(queue, [packages](const std::string& name, const Cancellation& cancel) {
        if (!packages) throw std::invalid_argument("Missing authenticated image packages");
        ReadResult result;
        if (cancel.Requested()) result.status = ReadStatus::Cancelled;
        else result.file = packages->Read(name, MaxSource);
        return result;
    }, std::move(root)) {
    if (!packages) { active = nullptr; throw std::invalid_argument("Missing authenticated image packages"); }
}
TextureService::TextureService(FileQueue& queue, Reader reader, std::string root) : state(new State(queue, std::move(reader), std::move(root))) {
    if (!state->device || !state->reader) throw std::invalid_argument("Images require an installed Gx device and reader");
    state->device->NativeCheckOwner();
    if (!state->diskRoot.empty()) {
        auto path = std::filesystem::canonical(std::filesystem::u8path(state->diskRoot));
        if (!std::filesystem::is_directory(path)) throw std::invalid_argument("Loose image root must be a directory");
        state->diskRoot = path.u8string();
    }
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
void TextureService::EnableAtlas(bool enable) { Check(); state->atlasEnabled = enable; }
void TextureService::SetFilter(uint32_t filter) {
    Check();
    if (filter > GxTex_LinearMipLinear) throw std::invalid_argument("Unavailable default image filter");
    state->filter = filter;
}
CTexture* CreateFileTexture(const char* filename, CGxTexFlags flags, CStatus* status, uint32_t options) {
    auto& service = Owner();
    auto& s = *service.state;
    if (s.closing) throw std::logic_error("Native image service is closing");
    // Original explicit/default sampler, disk, atlas, exact suffix and UI priority.
    if (!(options & 1)) flags.m_filter = s.filter;
    if (!status || (options & ~0x1Fu) || flags.m_filter > GxTex_LinearMipLinear ||
        flags.m_forceMipTracking || flags.m_generateMipMaps || flags.m_renderTarget || flags.m_bit8 ||
        flags.m_maxAnisotropy != 1 || flags.m_bit14 || flags.m_bit15 || flags.m_unk16 || flags.m_bit31)
        throw std::invalid_argument("Unavailable native file texture options/sampler");
    Key key{Filename(filename, options & 8), flags.m_filter, flags.m_wrapU, flags.m_wrapV,
            bool(options & 2), bool(options & 4) && s.atlasEnabled, !(options & 1)};
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
        auto root = s.diskRoot;
        image->ticket = s.queue.Submit([reader, root, image](const Cancellation& cancel) {
            auto result = ReadImage(reader, root, image->key, cancel);
            if (result.status != ReadStatus::Success || cancel.Requested()) return result;
            const auto& bytes = result.file.bytes;
            if (bytes.size() < 148 || bytes.size() > MaxSource) throw std::out_of_range("File image source extent");
            auto u32 = [&](size_t o) { return uint32_t(bytes[o]) | uint32_t(bytes[o+1]) << 8 |
                uint32_t(bytes[o+2]) << 16 | uint32_t(bytes[o+3]) << 24; };
            auto width = u32(12), height = u32(16);
            if (width < 8 || height < 8 || (width & (width - 1)) || (height & (height - 1)) ||
                uint64_t(width) * height > MaxPixels) throw std::out_of_range("File image dimensions/pixel budget");
            auto levels = Blizzard::Assets::DecodeBlp(bytes, image->key.filter < GxTex_NearestMipNearest);
            image->atlasSafe = bytes[11] & 0x10;
            // The original small single-level BLP rule supplies the default
            // policy. Explicit mip requests retain their complete-chain contract.
            image->smallDefault = image->key.defaultFilter && width < 256 && !u32(88);
            if (image->key.filter >= GxTex_NearestMipNearest && !image->smallDefault) {
                size_t count = 1;
                for (auto extent = std::max(width, height); extent > 1; extent >>= 1) ++count;
                if (levels.size() != count) throw std::invalid_argument("Mipped file texture requires a complete authored chain");
            } else levels.resize(1); // All declared spans/storage extents remain validated.
            image->Retain(std::move(levels));
            result.file.bytes.clear();
            result.file.bytes.shrink_to_fit();
            return result;
        }, [image, &s](const ReadResult& result) {
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
                if (image->smallDefault)
                    texture->m_gxTexFlags.m_filter = GxTex_Nearest;
                bool opaque = true;
                for (const auto& level : image->levels)
                    for (size_t i = 3; i < level.rgba.size(); i += 4)
                        if (level.rgba[i] != 255) { opaque = false; break; }
                texture->m_flags = (opaque ? 1 : 0) | (image->key.disk ? 2 : 0);
                texture->m_alphaBits = opaque ? 0 : 8;
                s.Attach(*image);
                if (texture->m_atlas) return;
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
    s.Detach(texture);
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
    return texture->m_atlas ? texture->m_atlas->m_gxTex : texture->m_gxTex;
}
}}

// Native atlas metadata uses the original public shape, but owns whole-sheet
// pixels through TextureService rather than legacy ResolveMipLevel/async objects.
CTextureAtlas::CTextureAtlas() : m_count(0), m_dirty(0), m_width(0), m_height(0),
    m_capacity(0), m_used(0), m_freeSlot(uint32_t(-1)), m_gxTex(nullptr),
    m_format(GxTex_Unknown), m_dataFormat(GxTex_Unknown), m_slots(64), m_textures{}, m_updateSlot(-1) {}
CTextureAtlas::~CTextureAtlas() { if (m_gxTex) GxTexDestroy(m_gxTex); }

void TextureEnableAtlas(int32_t enable) { Services::Native::Owner().EnableAtlas(enable != 0); }
void TextureSetFilterMode(EGxTexFilter filter) { Services::Native::Owner().SetFilter(uint32_t(filter)); }
int32_t TextureOnAtlas(CTexture* texture) {
    if (!texture) return 0;
    texture->NativeCheckOwner(); return texture->m_atlas != nullptr;
}
int32_t TextureGetAtlasOffsetAndScale(CTexture* texture, C2Vector& offset, float& scale) {
    if (!TextureOnAtlas(texture)) return 0;
    scale = 0.125f;
    offset.x = scale * (texture->m_atlasBlockIndex & 7);
    offset.y = scale * ((texture->m_atlasBlockIndex >> 3) & 7);
    return 1;
}
int32_t TextureIsSame(CTexture* texture, const char* name) {
    if (!texture) return 0;
    texture->NativeCheckOwner();
    return Services::Native::Filename(name) == texture->m_filename;
}

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
