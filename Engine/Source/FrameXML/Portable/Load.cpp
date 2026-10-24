// Native completed-read adaptation of original 12340 FrameXML.cpp
// FrameXML_ProcessFile / FrameXML_CreateFrames (0x00813EE0 / 0x00814340).
// Keep TOC order and hash bytes at the point of actual script/frame creation.
#include "Runtime.h"
#include "Factories.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CLayoutFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
namespace {
std::set<std::string> active;
bool Is(const char* a, const char* b) { return !SStrCmpI(a, b, STORM_MAX_STR); }
std::string Directory(const std::string& path) {
    auto slash = path.find_last_of("\\/");
    return slash == path.npos ? "" : path.substr(0, slash + 1);
}
struct FileScope {
    std::string path;
    explicit FileScope(const char* value) : path(value ? value : "") {
        std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; });
        if (active.size() >= 64 || !active.insert(path).second)
            throw std::invalid_argument("Cyclic or excessive FrameXML Include graph: " + path);
    }
    ~FileScope() { active.erase(path); }
};
[[noreturn]] void Failure(const std::string& path, CStatus& status) {
    char text[4096];
    throw std::runtime_error(path + ": " + status.GetErrorStr(text, sizeof(text), STATUS_WARNING, "; "));
}
}
int32_t FrameXML_ProcessFile(const char* filename, const char* wrapper, MD5_CTX* md5, CStatus* status) {
    FrameXML::Native::RequireOwner();
    FileScope scope(filename);
    const char* extension = std::strrchr(filename, '.');
    if (extension && Is(extension, ".lua")) return FrameScript_ExecuteFile(filename, wrapper, md5, status);
    std::unique_ptr<XMLTree, decltype(&XMLTree_Free)> tree(FrameXML_LoadXML(filename, md5, status), XMLTree_Free);
    if (!tree) return 0;
    CStatus fileStatus;
    for (auto* child = XMLTree_GetRoot(tree.get())->m_child; child;) {
        auto* next = child->m_next; // virtual nodes detach from this tree
        const char* tag = child->m_name.m_str;
        if (Is(tag, "Include") || Is(tag, "Script")) {
            const char* file = child->GetAttributeByName("file");
            if (file) {
                auto path = Directory(filename) + file;
                const bool loaded = Is(tag, "Include") ? FrameXML_ProcessFile(path.c_str(), wrapper, md5, &fileStatus)
                    : FrameScript_ExecuteFile(path.c_str(), wrapper, md5, &fileStatus);
                if (!loaded) Failure(path, fileStatus);
            } else if (Is(tag, "Include")) {
                throw std::runtime_error(std::string(filename) + ": Include missing file attribute");
            }
            if (Is(tag, "Script") && child->m_body && *child->m_body) {
                const auto name = std::string(filename) + ":<Scripts>";
                if (!FrameScript_ExecuteBuffer(child->m_body, child->m_bodyLen, name.c_str(), &fileStatus, nullptr))
                    Failure(name, fileStatus);
            }
        } else if (Is(tag, "Font")) {
            const char* name = child->GetAttributeByName("name");
            if (!name || !*name) throw std::runtime_error(std::string(filename) + ": Unnamed font");
            CSimpleFont::GetFont(name, 1)->LoadXML(child, &fileStatus);
        } else {
            const char* isVirtual = child->GetAttributeByName("virtual");
            if (isVirtual && Is(isVirtual, "true")) {
                const char* name = child->GetAttributeByName("name");
                if (!name || !*name) throw std::runtime_error(std::string(filename) + ": Unnamed virtual object");
                FrameXML_StoreHashNode(child, name, lua_tainted, &fileStatus);
            } else {
                try {
                    if (!FrameXML_CreateFrame(child, nullptr, &fileStatus)) Failure(filename, fileStatus);
                    CLayoutFrame::ResizePending();
                } catch (const std::exception& error) {
                    const char* name = child->GetAttributeByName("name");
                    throw std::runtime_error(std::string(filename) + ": " + tag + " " + (name ? name : "(unnamed)") + ": " + error.what());
                }
            }
        }
        if (fileStatus.m_highestSeverity >= STATUS_ERROR) Failure(filename, fileStatus);
        child = next;
    }
    if (status) status->Add(fileStatus);
    return 1;
}
int32_t FrameXML_CreateFrames(const char* tocFile, const char* wrapper, MD5_CTX* md5, CStatus* status) {
    FrameXML::Native::RequireOwner();
    CStatus fallback;
    if (!status) status = &fallback;
    FileScope scope(tocFile);
    std::string path; std::vector<uint8_t> bytes;
    if (!FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Interface, tocFile, path, bytes)) {
        status->Add(STATUS_ERROR, "Couldn't open %s", tocFile);
        return 0;
    }
    if (md5 && !bytes.empty()) MD5Update(md5, bytes.data(), static_cast<uint32_t>(bytes.size()));
    if (std::find(bytes.begin(), bytes.end(), 0) != bytes.end()) throw std::runtime_error("NUL in FrameXML TOC");
    std::string text(bytes.begin(), bytes.end());
    const char* cursor = text.c_str();
    if (bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) cursor += 3;
    char line[1024];
    do {
        const char* previous = cursor;
        SStrTokenize(&cursor, line, sizeof(line), "\r\n", nullptr);
        if (cursor - previous >= 1024) throw std::runtime_error("FrameXML TOC line exceeds original buffer");
        if (!*line) break;
        if (*line == '#') continue;
        std::string member = Directory(path) + line;
        while (!member.empty() && member.back() == ' ') member.pop_back();
        if (!FrameXML_ProcessFile(member.c_str(), wrapper, md5, status)) Failure(member, *status);
    } while (*cursor);
    return 1;
}
