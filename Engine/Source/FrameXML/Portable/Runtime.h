#ifndef WOWDECOMP_NATIVE_XML_RUNTIME_H
#define WOWDECOMP_NATIVE_XML_RUNTIME_H
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include <array>
#include <cstddef>
#include <cstdint>
class CStatus;
struct MD5_CTX;
struct XMLTree;

namespace FrameXML { namespace Native {
// Shares the installed FrameScript owner and its completed Interface reader.
// Close all trees/borrowed nodes before this owner, then close FrameScript.
class Runtime final {
public:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
private:
    FrameScript::Native::Execution lifetime;
};
struct OwnerLease final {
    OwnerLease();
    ~OwnerLease();
    OwnerLease(const OwnerLease&) = delete;
    OwnerLease& operator=(const OwnerLease&) = delete;
};
void RequireNoConsumers();
void RequireOwner();
void RequireNoTrees();
void TreeCreated();
void TreeFreed();
// Fixed original 12340 InterfaceKey; no application alternate-key option.
const uint8_t* OriginalInterfaceKey();
struct TraversalStats { size_t files = 0, bytes = 0; };
TraversalStats LastTraversal();
void SetTraversal(TraversalStats);
}}
// Original result values: 0 unavailable, 1 invalid RSA/envelope,
// 2 modified content (including incomplete native traversal), 3 authenticated.
int32_t FrameXML_CheckSignature(const char* tocFile, const char* extraFile,
                              const uint8_t* modulus, uint8_t* digest);
XMLTree* FrameXML_LoadXML(const char* filename, MD5_CTX* md5, CStatus* status);
#endif
