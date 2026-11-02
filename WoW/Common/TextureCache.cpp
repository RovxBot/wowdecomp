#include <cstddef>
#include <cstdint>
#include <new>
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
#include <cstring>
#include <stdexcept>
#endif
#include "WoW/Common/TextureCache.h"
#include "Engine/Source/Base/ObjectAlloc.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Services/Types.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00B6BA48
static HASHKEY_NONE s_cacheKey;

// STATIC: 0x00B6BA4C
static uint32_t* s_entryHeap;

// STATIC: 0x00B6BA50
static int32_t s_loadCount;

// STATIC: 0x00B6BA54
static TSHashTable<CACHEENTRY, HASHKEY_NONE> s_cacheTable;

// VERIFY: dead4cbd manual
// OFFSET: 0x004F2B40
static void FreeRequest(CAsyncObject* object) {
    void* buffer = object->m_buffer;
    AsyncFileReadDestroyObject(object);
    SMemFree(buffer, __FILE__, __LINE__, 0);
}

// VERIFY: d664e324 manual
// OFFSET: 0x004F2B70
static void LoadSuccessCallback(void* param) {
    CACHEENTRY* entry = static_cast<CACHEENTRY*>(param);

    AsyncFileReadDestroyObject(entry->m_asyncObject);
    BLPHeader* header = entry->m_data;
    entry->m_asyncObject = nullptr;
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    try {
        if (!header->m_width || !header->m_height ||
            uint64_t(header->m_width) * header->m_height > 2 * 1024 * 1024)
            throw std::runtime_error("Character texture exceeds native retained pixel extent");
        const auto* begin = reinterpret_cast<const uint8_t*>(entry->m_data);
        Blizzard::Assets::Bytes bytes(begin,begin+entry->m_size);
        const auto levels = Blizzard::Assets::DecodeBlp(bytes);
        if (levels.empty() || levels.front().width > UINT16_MAX || levels.front().height > UINT16_MAX ||
            levels.size() != size_t(TextureCalcMipCount(levels.front().width,levels.front().height)))
            throw std::runtime_error("Original character texture requires a complete mip chain");
    } catch (...) { entry->m_loadFailed = 1; throw; }
#endif
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (!CBLPFile::ValidateHeader(*header)) {
        entry->m_loadFailed = 1;
        throw std::runtime_error("Invalid original character texture header");
    }
#else
    CBLPFile::ValidateHeader(*header);
#endif

    entry->m_info.m_alphaSize = header->m_alphaSize;
    entry->m_info.m_opaque = header->m_alphaSize == 0;
    entry->m_info.m_width = header->m_width;
    entry->m_info.m_height = header->m_height;
    entry->m_info.m_mipCount = TextureCalcMipCount(entry->m_info.m_width, entry->m_info.m_height);
}

#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
static void LoadFailureCallback(void* param) {
    auto* entry = static_cast<CACHEENTRY*>(param);
    auto* object = entry->m_asyncObject;
    entry->m_asyncObject = nullptr;
    entry->m_loadFailed = 1;
    AsyncFileReadDestroyObject(object);
}
#endif

// VERIFY: ca8c7168 manual
// OFFSET: 0x004F2BE0
int32_t CACHEENTRY::LoadTexture() {
    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, this->m_fileName, 0, &file)) {
        char alternate[260];
        TEXFILETYPE type = TextureDiscoverFileType(this->m_fileName);
        TexturePickAlternateFilename(this->m_fileName, type, alternate, sizeof(alternate));
        SFile::OpenEx(nullptr, alternate, 0, &file);
    }

    if (!file) {
        this->m_loadFailed = 1;
        return 0;
    }

    this->m_asyncObject = AsyncFileReadAllocObject();
    this->m_asyncObject->m_userArg = this;
    this->m_asyncObject->m_userPostloadCallback = reinterpret_cast<void*>(&LoadSuccessCallback);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    this->m_asyncObject->m_userPostUnloadCallback = reinterpret_cast<void*>(&LoadFailureCallback);
#endif
    this->m_asyncObject->m_file = file;
    this->m_asyncObject->m_size = SFileGetFileSize(file, nullptr);
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (m_asyncObject->m_size < 148 || m_asyncObject->m_size > 32 * 1024 * 1024) {
        m_loadFailed = 1;
        AsyncFileReadDestroyObject(m_asyncObject); m_asyncObject = nullptr;
        return 0;
    }
#endif
    this->m_asyncObject->m_priority = 130;
    this->m_size = this->m_asyncObject->m_size;
    this->m_data = static_cast<BLPHeader*>(SMemAlloc(this->m_size, __FILE__, __LINE__, 0));
    this->m_asyncObject->m_buffer = this->m_data;
    AsyncFileReadObject(this->m_asyncObject, 0);

    return 1;
}

// VERIFY: 70cae207 manual
// OFFSET: 0x004F2CE0
HTEXTURECACHE TextureCacheCopyTexture(HTEXTURECACHE handle) {
    CACHEENTRY* entry = handle;

    if (entry) {
        entry->m_refCount++;
    }

    return entry;
}

// VERIFY: 7bec2c4f manual
// OFFSET: 0x004F2D00
void* TextureCacheGetMip(HTEXTURECACHE handle, uint32_t mipLevel) {
    CACHEENTRY* entry = handle;

    if (!entry || entry->m_loadFailed || !entry->m_data || mipLevel >= entry->m_info.m_mipCount) {
        return nullptr;
    }

    return reinterpret_cast<uint8_t*>(entry->m_data) + entry->m_data->m_mipOffsets[mipLevel];
}

// VERIFY: 0bb1b277 manual
// OFFSET: 0x004F2D40
BlpPalPixel* TextureCacheGetPal(HTEXTURECACHE handle) {
    CACHEENTRY* entry = handle;

    if (!entry || entry->m_loadFailed || !entry->m_data || entry->m_data->m_colorEncoding != COLOR_PAL) {
        return nullptr;
    }

    return entry->m_data->m_extended.m_palette;
}

// VERIFY: 8b89ff06 manual
// OFFSET: 0x004F2D80
int32_t TextureCacheHasMips(HTEXTURECACHE handle) {
    CACHEENTRY* entry = handle;

    return entry && entry->m_data && !entry->m_loadFailed;
}

// VERIFY: 195da797 auto
// OFFSET: 0x004F2DB0
void TextureCacheResetLoadCount() {
    s_loadCount = 0;
}

// VERIFY: c060d573 auto
// OFFSET: 0x004F2DC0
void TextureCacheComputeLoadProgress(HTEXTURECACHE handle, int64_t& local, int64_t& total) {
    CACHEENTRY* entry = handle;

    if (!entry) {
        return;
    }

    if (!entry->m_loadFailed && !entry->m_data) {
        entry->LoadTexture();
    }

    SFile::FileGetIsLocalAmount(entry->m_fileName, local, total);
    total++;

    if (!entry->m_asyncObject) {
        local++;
    }
}

// VERIFY: 89bf2f8e manual
// OFFSET: 0x004F2E20
void TextureCacheDestroy() {
    delete s_entryHeap;
    s_entryHeap = nullptr;
}

// VERIFY: 65c3dd55 auto
// OFFSET: 0x004F2E50
int32_t TextureCacheGetInfo(HTEXTURECACHE handle, TCTEXTUREINFO& info, int32_t wait) {
    CACHEENTRY* entry = handle;

    if (!entry) {
        return 0;
    }

    if (!entry->m_loadFailed) {
        if (!entry->m_data) {
            entry->LoadTexture();
        }

        if (!entry->m_loadFailed && !entry->m_info.m_width && entry->m_asyncObject) {
            if (!wait) {
                if (SFile::IsStreamingMode()) {
                    AsyncFile::EnterQueueLock();
                    CAsyncObject* object = entry->m_asyncObject;

                    if (object->IsPending()) {
                        AsyncFileReadIncreasePriority(object);
                    }

                    AsyncFile::LeaveQueueLock();
                }

                return 0;
            }

            AsyncFileReadWait(entry->m_asyncObject);
        }
    }

#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (entry->m_loadFailed || !entry->m_info.m_width || !entry->m_info.m_height) return 0;
#endif
    info = entry->m_info;

    return 1;
}

// VERIFY: 3bb1eceb manual
// OFFSET: 0x004F2EF0
CACHEENTRY::~CACHEENTRY() {
    int32_t freeData = 1;

    if (this->m_asyncObject) {
        freeData = AsyncFile::Cancel(this->m_asyncObject, &FreeRequest);
        this->m_asyncObject = nullptr;
    }

    if (this->m_data && freeData) {
        SMemFree(this->m_data, __FILE__, __LINE__, 0);
    }

    this->m_size = 0;
    this->m_data = nullptr;
    this->m_fileName[0] = '\0';
}

// VERIFY: 26c07029 manual
// OFFSET: 0x004F2F60
CACHEENTRY::CACHEENTRY() {
    this->m_info.m_mipCount = 0;
    this->m_info.m_opaque = 1;
    this->m_info.m_width = 0;
    this->m_info.m_height = 0;
    this->m_info.m_alphaSize = 0;
    this->m_size = 0;
    this->m_loadFailed = 0;
    this->m_asyncObject = nullptr;
    this->m_fileName[0] = '\0';
    this->m_refCount = 0;
    this->m_memHandle = 0;
    this->m_data = nullptr;
}

// VERIFY: 5cf66dc0 manual
// OFFSET: 0x004F3110
static CACHEENTRY* TextureCacheAllocEntry() {
    if (!s_entryHeap) {
        s_entryHeap = new (__FILE__, __LINE__) uint32_t(ObjectAllocAddHeap(sizeof(CACHEENTRY), 1024, "TCACHEENTRY", true));
    }

    uint32_t memHandle;
    void* object = nullptr;

    if (!ObjectAlloc(*s_entryHeap, &memHandle, &object, false)) {
        return nullptr;
    }

    CACHEENTRY* entry = new (object) CACHEENTRY();
    entry->m_memHandle = memHandle;

    return entry;
}

// VERIFY: 52f7899b auto
// OFFSET: 0x004F31A0
void TextureCacheDestroyTexture(HTEXTURECACHE handle) {
    CACHEENTRY* entry = handle;

    if (!entry) {
        return;
    }

    if (--entry->m_refCount <= 0) {
        s_cacheTable.Unlink(entry);
        uint32_t* heap = s_entryHeap;
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
        const auto index = entry->m_memHandle;
#endif
        entry->~CACHEENTRY();
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
        ObjectFree(*heap, index);
#else
        ObjectFree(*heap, entry->m_memHandle);
#endif
    }
}

// VERIFY: 1e1d17fd auto
// OFFSET: 0x004F3930
HTEXTURECACHE TextureCacheCreateTexture(const char* fileName) {
#if defined(WOWDECOMP_NATIVE_TEST_SERVER_LOGIN)
    if (!fileName || !*fileName || std::strlen(fileName) >= sizeof(CACHEENTRY::m_fileName))
        throw std::invalid_argument("Character texture path exceeds original cache storage");
#endif
    uint32_t hashval = SStrHash(fileName, 0, 0);
    CACHEENTRY* entry = s_cacheTable.Ptr(hashval, s_cacheKey);

    if (entry) {
        entry->m_refCount++;
        return entry;
    }

    entry = TextureCacheAllocEntry();
    s_cacheTable.Insert(entry, hashval, s_cacheKey);
    SStrCopy(entry->m_fileName, fileName, sizeof(entry->m_fileName));
    entry->m_refCount++;

    return entry;
}
