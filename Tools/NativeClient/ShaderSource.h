#ifndef WOWDECOMP_NATIVE_CLIENT_SHADER_SOURCE_H
#define WOWDECOMP_NATIVE_CLIENT_SHADER_SOURCE_H
#include "Engine/Source/Services/Portable/AsyncFile.h"

// FileQueue retains Packages errors as text. Only its exact absent-member
// disposition permits original D3D profile descent. Signature/codec/I/O failures,
// patch deletions, cancellations and an empty existing file remain fatal.
inline bool MissingShaderRead(const Services::Native::ReadResult& result, const std::string& path) {
    return result.status == Services::Native::ReadStatus::Failed &&
        result.error == "MPQ member not found: " + Blizzard::Mopaq::Native::Name(path);
}
#endif
