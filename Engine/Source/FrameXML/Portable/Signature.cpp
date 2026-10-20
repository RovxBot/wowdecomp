#include "Runtime.h"
#include "InterfaceKey.h"
#include "Engine/Source/BlizzardCore/Portable/Package.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/Storm/H/storm.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <string_view>

// Native adaptation of LoadXML.cpp's 12340 Process*ForMakeSign (0x00816060-
// 0x008164D0) and FrameXML_CheckSignature (0x008165E0). The raw manifest scanner
// is intentional: using a semantic XML walk would change the signed byte order.
// These are native bounds/ownership adaptations, not new OFFSET/VERIFY claims.
namespace {
using Bytes = Blizzard::Mopaq::Native::Bytes;
bool White(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
std::string Upper(std::string value) {
    for (char& c : value) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return value;
}
bool Equal(std::string_view a, const char* b) { return Upper(std::string(a)) == b; }
bool Read(const std::string& name, std::string& path, Bytes& bytes) {
    return FrameScript::Native::ReadSource(FrameScript::Native::SourceKind::Interface,
                                           name.c_str(), path, bytes);
}
struct Traversal {
    MD5_CTX md5;
    FrameXML::Native::TraversalStats stats;
    std::set<std::string> active;
    Traversal() { MD5Init(&md5); }
    bool Feed(const Bytes& bytes) {
        if (++stats.files > 16384 || bytes.size() > 256 * 1024 * 1024u - stats.bytes) return false;
        stats.bytes += bytes.size();
        if (!bytes.empty()) MD5Update(&md5, bytes.data(), static_cast<uint32_t>(bytes.size()));
        return true;
    }
    bool Tag(std::string_view tag, const std::string& base) {
        size_t name = 0;
        while (name < tag.size() && White(tag[name])) ++name;
        size_t end = name;
        while (end < tag.size() && !White(tag[end])) ++end;
        if (end == tag.size() || (!Equal(tag.substr(name, end-name), "SCRIPT") &&
                                 !Equal(tag.substr(name, end-name), "INCLUDE"))) return true;
        // Match the original scanner's double-quote/whitespace/slash rules,
        // including its raw scan of tags inside comments. Do not use XMLTree.
        while (end < tag.size()) {
            size_t attribute = end + 1;
            while (attribute < tag.size() && White(tag[attribute])) ++attribute;
            end = attribute;
            while (end < tag.size() && !White(tag[end]) && tag[end] != '=') ++end;
            if (end == tag.size()) return true;
            if (!Equal(tag.substr(attribute, end-attribute), "FILE")) continue;
            size_t value = end + 1;
            while (value < tag.size() && (White(tag[value]) || tag[value] == '=' || tag[value] == '"')) ++value;
            size_t finish = value;
            while (finish < tag.size() && tag[finish] != '\r' && tag[finish] != '\n' &&
                   tag[finish] != '\t' && tag[finish] != '"' && tag[finish] != '/') ++finish;
            return value < tag.size() && File(std::string(tag.substr(value, finish-value)), base);
        }
        return true;
    }
    bool File(std::string name, const std::string& base) {
        if (name.empty() || name.front() == '\\' || name.front() == '/' || name.find(':') != name.npos) return false;
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (name.empty() || active.size() >= 64 || stats.files >= 16384) return false;
        Bytes bytes; std::string path;
        if (!Read(base + name, path, bytes)) return false;
        const std::string identity = Upper(path);
        if (!active.insert(identity).second) return false;
        struct Pop { std::set<std::string>& set; const std::string& name; ~Pop() { set.erase(name); } } pop{active, identity};
        if (!Feed(bytes)) return false;
        size_t slash = path.find_last_of('\\');
        std::string directory = slash == path.npos ? "" : path.substr(0, slash+1);
        size_t dot = name.find_last_of('.');
        std::string extension = dot == name.npos ? "" : Upper(name.substr(dot));
        if (extension != ".TOC" && extension != ".XML") return true;
        // Original text readers are NUL terminated. Embedded NUL is rejected
        // instead of silently leaving bytes outside the manifest traversal.
        if (std::find(bytes.begin(), bytes.end(), 0) != bytes.end()) return false;
        std::string text(bytes.begin(), bytes.end());
        if (extension == ".TOC") {
            const char* current = text.c_str();
            char line[1024];
            do {
                const char* before = current;
                SStrTokenize(&current, line, sizeof(line), "\r\n", nullptr);
                if (current - before >= 1024) return false;
                if (!line[0]) break;
                if (line[0] != '#' && !File(line, directory)) return false;
            } while (*current);
        } else {
            size_t cursor = text.find('<');
            while (cursor != text.npos) {
                size_t end = text.find('>', cursor+1);
                if (end == text.npos) break;
                if (!Tag(std::string_view(text).substr(cursor+1, end-cursor-1), directory)) return false;
                cursor = text.find('<', end+1);
            }
        }
        return true;
    }
};
}
namespace FrameXML { namespace Native {
const uint8_t* OriginalInterfaceKey() { return interfaceKey; }
}}
int32_t FrameXML_CheckSignature(const char* tocFile, const char* extraFile,
                              const uint8_t* modulus, uint8_t* digest) {
    FrameXML::Native::RequireOwner();
    FrameXML::Native::SetTraversal({});
    if (!digest) return 0;
    std::memset(digest, 0, 16);
    if (!tocFile || !*tocFile) return 0;
    Bytes signature; std::string path;
    if (!Read(std::string(tocFile) + ".sig", path, signature)) return 0;
    if (signature.size() != 276 || !modulus) return 1;
    size_t slash = path.find_last_of('\\');
    std::string filename = Upper(path.substr(slash == path.npos ? 0 : slash+1));
    // SSignatureVerifyStream hashes the 16-byte signed digest and uppercase
    // basename, then verifies the original NGIS + 2048-bit little-endian RSA.
    Bytes envelope(signature.begin(), signature.begin()+16);
    envelope.insert(envelope.end(), filename.begin(), filename.end());
    envelope.insert(envelope.end(), signature.begin()+16, signature.end());
    if (!Blizzard::Mopaq::Native::VerifySignature(envelope, Bytes(modulus, modulus+256))) return 1;
    Traversal traversal;
    bool complete = traversal.File(tocFile, "");
    if (complete && extraFile) {
        Bytes bytes;
        complete = Read(extraFile, path, bytes) && traversal.Feed(bytes);
    }
    FrameXML::Native::SetTraversal(traversal.stats);
    MD5Final(digest, &traversal.md5);
    return complete && !std::memcmp(signature.data(), digest, 16) ? 3 : 2;
}
