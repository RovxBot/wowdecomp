# Read-only 12340 MPQ packages. Only audited bundled codecs enter this graph.
add_library(WowAssetCodecs STATIC
    ../../../ThirdParty/zlib/adler32.c
    ../../../ThirdParty/zlib/crc32.c
    ../../../ThirdParty/zlib/inffast.c
    ../../../ThirdParty/zlib/inflate.c
    ../../../ThirdParty/zlib/inftrees.c
    ../../../ThirdParty/zlib/zutil.c
    ../../../ThirdParty/zlib/contrib/blast/blast.c
    ../../../ThirdParty/bzip2/blocksort.c
    ../../../ThirdParty/bzip2/bzlib.c
    ../../../ThirdParty/bzip2/compress.c
    ../../../ThirdParty/bzip2/crctable.c
    ../../../ThirdParty/bzip2/decompress.c
    ../../../ThirdParty/bzip2/huffman.c
    ../../../ThirdParty/bzip2/randtable.c
    ../../../ThirdParty/lzma/LzmaDecode.c
)
target_include_directories(WowAssetCodecs PRIVATE ${PROJECT_SOURCE_DIR}/ThirdParty/zlib)
target_link_libraries(WowAssetCodecs PUBLIC WowCoreServices)
add_library(WowNativeAssets STATIC
    ../BlizzardCore/Source/Packages/Mopaq/MPQEncrypt.cpp
    ../BlizzardCore/Portable/PackageArchive.cpp
    ../BlizzardCore/Portable/PackageCodecs.cpp
    ../BlizzardCore/Portable/PackageSignatures.cpp
    ../BlizzardCore/Portable/Packages.cpp
)
target_link_libraries(WowNativeAssets PUBLIC WowNativeFiles WowPortableCore PRIVATE WowAssetCodecs)
if(BUILD_TESTING)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Tools/PortableAssets ${PROJECT_BINARY_DIR}/Tools/PortableAssets)
endif()
