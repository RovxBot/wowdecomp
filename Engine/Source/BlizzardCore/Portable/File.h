#ifndef WOWDECOMP_PORTABLE_FILE_H
#define WOWDECOMP_PORTABLE_FILE_H

#include "Engine/Source/BlizzardCore/Source/System/File/Types.h"
#include <string>

// Host-ABI file subset. No Windows handles, archive/streaming layer, or binary
// verification claim. Stream lifetime belongs to the caller; stop users before
// Close. Operations on a live stream are serialized, including its cursor.
namespace Blizzard { namespace File {
bool Open(const char* filename, int32_t flags, StreamRecord*& file);
bool Close(StreamRecord* file);
bool Flush(StreamRecord* file);
bool Read(StreamRecord* file, void* data, int32_t* count);
bool Read(StreamRecord* file, void* data, int64_t offset, int32_t* count);
bool Write(StreamRecord* file, const void* data, int32_t* count);
bool Write(StreamRecord* file, const void* data, int64_t offset, int32_t* count);
bool GetPos(StreamRecord* file, int64_t* offset);
bool SetPos(StreamRecord* file, int64_t offset, int32_t whence);
bool SetEOF(StreamRecord* file, int64_t offset, int32_t whence);
FileInfo* GetFileInfo(StreamRecord* file);
bool GetFileInfo(StreamRecord* file, FileInfo* info);
bool GetFileInfo(const char* path, FileInfo* info);
int32_t Exists(const char* path);
bool IsDirectory(const char* path);
bool IsFile(const char* path);
bool CreateDirectory(const char* path, bool recursive);
bool RemoveDirectory(const char* path);
bool RemoveDirectoryAndContents(const char* path, bool removeReadOnly);
bool Delete(const char* path);
bool Move(const char* src, const char* dst);
bool Copy(const char* src, const char* dst, bool overwrite);
bool ProcessDirFast(const char* path, void* cookie, ProcessDirCallback callback, bool treatSymlinksAsFiles);
bool GetWorkingDirectory(char* buffer, int32_t size);
bool SetWorkingDirectory(const char* path);
bool MakeAbsolutePath(const char* path, char* result, int32_t size, bool canonicalize);
bool IsAbsolutePath(const char* path);
int32_t GetRootChars(const char* path);
bool SetAttributes(const char* path, int32_t attributes);
bool SimpleGlob(const char* name, const char* pattern);

namespace Native {
enum class Disposition { CreateNew, CreateAlways, OpenExisting, OpenAlways, TruncateExisting };
// access/share use the File API's low bits: read=1, write=2, share read=4,
// share write=8. POSIX share bits are hints, not mandatory Windows share locks.
bool Open(const char* path, int32_t accessAndShare, Disposition disposition, StreamRecord*& file);

struct Paths { std::string data; std::string settings; std::string cache; };
enum class UserRoot { Settings, Cache };
// Explicit host roots, never MPQ-internal paths. Creates only the writable roots;
// rejects overlapping roots after resolving existing symlinks.
bool ConfigurePaths(const char* data, const char* settings, const char* cache, Paths& result);
// ASCII case-insensitive compatibility lookup rejects all ambiguous components,
// even when one spelling matches exactly. Non-ASCII names retain exact UTF-8.
bool ResolveDataPath(const Paths& paths, const char* relative, std::string& result);
bool OpenData(const Paths& paths, const char* relative, StreamRecord*& file);
// Resolves only; does not create parent directories or change the data tree.
bool ResolveUserPath(const Paths& paths, UserRoot root, const char* relative, std::string& result);
}
}}
#endif
