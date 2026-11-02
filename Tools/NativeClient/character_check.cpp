// Authored raw-cache ownership/extent fixtures for the newly selected character
// producer. They contain no game assets and do not qualify character appearance.
#include "WoW/Common/TextureCache.h"
#include "WoW/Source/ObjectAlloc/ObjectAlloc.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Services/Portable/LegacyFiles.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
namespace {
using Bytes = std::vector<uint8_t>;
void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
Bytes LargeImage() {
    Bytes bytes(148);
    const auto put=[&](size_t offset,uint32_t value) {
        for (unsigned i=0;i<4;++i) bytes[offset+i]=uint8_t(value>>(i*8));
    };
    put(0,0x32504c42); put(4,1); bytes[8]=3; bytes[9]=8; bytes[11]=1;
    put(12,512); put(16,512);
    for (uint32_t level=0,side=512;;++level,side>>=1) {
        const uint32_t size=side*side*4;
        put(20+level*4,bytes.size()); put(84+level*4,size);
        bytes.resize(bytes.size()+size,0x73);
        if (side==1) break;
    }
    return bytes;
}
}
void CheckNativeCharacterCache() {
    using namespace Services::Native;
    FileQueue queue;
    const auto authored=LargeImage();
    LegacyFiles files(queue,[&](const std::string& path,const Cancellation&) {
        ReadResult result;
        if (path.find("LARGE.BLP")!=std::string::npos) result.file.bytes=authored;
        else if (path.find("SHORT.BLP")!=std::string::npos) result.file.bytes={0};
        else result.file.bytes.resize(148);
        return result;
    });
    for (unsigned cycle=0;cycle<3;++cycle) {
        auto* large=TextureCacheCreateTexture("large.blp");
        TCTEXTUREINFO info{};
        Check(TextureCacheGetInfo(large,info,1) && info.m_width==512 && info.m_mipCount==10,
              "Complete authored character texture did not load");
        Check(large->m_size==authored.size() && large->m_size>(1u<<20) && TextureCacheGetMip(large,9),
              "Character raw cache truncated its allocation or mip extent");
        TextureCacheDestroyTexture(large);
        auto* shortFile=TextureCacheCreateTexture("short.blp");
        Check(!TextureCacheGetInfo(shortFile,info,1) && shortFile->m_loadFailed && !shortFile->m_asyncObject,
              "Truncated character image became a loaded cache entry");
        TextureCacheDestroyTexture(shortFile);
        auto* malformed=TextureCacheCreateTexture("malformed.blp");
        bool rejected=false;
        try { (void)TextureCacheGetInfo(malformed,info,1); } catch (const std::exception&) { rejected=true; }
        Check(rejected && malformed->m_loadFailed && !malformed->m_asyncObject,
              "Malformed character texture was approved or retained a callback");
        TextureCacheDestroyTexture(malformed);
        auto* cancelled=TextureCacheCreateTexture("cancelled.blp");
        (void)TextureCacheGetInfo(cancelled,info,0);
        TextureCacheDestroyTexture(cancelled);
        files.Poll(); files.CheckCanClose();
        TextureCacheDestroy(); ObjectAllocDestroy();
    }
    std::cout<<"native-character-cache-wide-extent-mips-malformed-cancel-release=passed\n";
}
