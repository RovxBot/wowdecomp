#include "Runtime.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/FrameScript/Portable/Runtime.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include <stdexcept>

namespace FrameXML { namespace Native {
namespace {
bool installed = false;
size_t trees = 0;
TraversalStats last;
}
Runtime::Runtime() {
    FrameScript::Native::RequireOwner();
    if (installed) throw std::logic_error("Only one XML runtime may be installed");
    installed = true;
    last = {};
}
Runtime::~Runtime() {
    try {
        RequireNoTrees();
        XMLTree_Cleanup();
        CStringManager::DestroyManager();
        XMLNode::s_allocator.Clear(nullptr, 0, 1);
        installed = false;
    } catch (...) { std::terminate(); }
}
void RequireOwner() {
    // FrameScript's atomic owner publication also rejects foreign threads before
    // reading any of our owner-thread globals.
    FrameScript::Native::RequireOwner();
    if (!installed) throw std::logic_error("Native XML runtime is not installed");
}
void RequireNoTrees() {
    RequireOwner();
    if (trees) throw std::logic_error("Close XML trees before parser/pool cleanup");
}
void TreeCreated() { RequireOwner(); ++trees; }
void TreeFreed() { RequireOwner(); --trees; }
TraversalStats LastTraversal() { RequireOwner(); return last; }
void SetTraversal(TraversalStats value) { RequireOwner(); last = value; }
}}

// Native completed-read adaptation of FrameXML.cpp::FrameXML_LoadXML. Feed the
// original bytes before parsing; the parser owns copied nodes, never read tickets.
XMLTree* FrameXML_LoadXML(const char* filename, MD5_CTX* md5, CStatus* status) {
    FrameXML::Native::RequireOwner();
    std::string path;
    std::vector<uint8_t> bytes;
    if (!FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Interface, filename, path, bytes)) {
        if (status) status->Add(STATUS_ERROR, "Couldn't open %s", filename ? filename : "(null)");
        return nullptr;
    }
    if (md5 && !bytes.empty()) MD5Update(md5, bytes.data(), static_cast<uint32_t>(bytes.size()));
    XMLTree* tree = XMLTree_Load(reinterpret_cast<const char*>(bytes.data()), static_cast<uint32_t>(bytes.size()));
    if (!tree && status) status->Add(STATUS_ERROR, "Couldn't parse XML in %s", path.c_str());
    return tree;
}
