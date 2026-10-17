#include "ShellSettings.h"
#include "Engine/Source/Os/OsSecureRandom.h"
#include "Engine/Source/Os/Portable/Window.h"
#include "Engine/Source/Storm/H/storm.h"
#include <charconv>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace Services {
namespace Native {
namespace File = Blizzard::File;
namespace {
std::string Path(const File::Native::Paths &paths, const char *name) {
    std::string result;
    if (!File::Native::ResolveUserPath(paths, File::Native::UserRoot::Settings, name, result))
        throw std::runtime_error("Cannot resolve native shell settings path");
    return result;
}
void Validate(const ShellSettings &settings) {
    if (settings.width < 320 || settings.width > 8192 || settings.height < 240 || settings.height > 8192 ||
        settings.text.size() > 4096)
        throw std::runtime_error("Invalid native shell settings value");
    OsNative::DecodeUtf8(settings.text);
}
struct Stream {
    File::StreamRecord *file = nullptr;
    ~Stream() {
        if (file)
            File::Close(file);
    }
    void Close() {
        auto value = file;
        file = nullptr;
        if (!File::Close(value))
            throw std::runtime_error("Cannot close native shell settings");
    }
};
template <class T> T Number(const std::string &text) {
    T value{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
        throw std::runtime_error("Invalid native shell settings number");
    return value;
}
unsigned Hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    throw std::runtime_error("Invalid native shell settings UTF-8 encoding");
}
} // namespace
ShellSettings LoadSettings(const File::Native::Paths &paths) {
    auto path = Path(paths, "native-shell.settings");
    Stream stream;
    if (!File::Native::Open(path.c_str(), 1 | 4, File::Native::Disposition::OpenExisting, stream.file)) {
        if (SErrGetLastError() == 2)
            return {};
        throw std::runtime_error("Cannot read native shell settings");
    }
    File::FileInfo info{};
    if (!File::GetFileInfo(stream.file, &info) || info.m_size < 0 || info.m_size > 16384)
        throw std::runtime_error("Invalid native shell settings size");
    std::string content(static_cast<size_t>(info.m_size), '\0');
    size_t completed = 0;
    while (completed < content.size()) {
        int32_t size = static_cast<int32_t>(content.size() - completed);
        if (!File::Read(stream.file, content.data() + completed, &size) || size <= 0)
            throw std::runtime_error("Short native shell settings read");
        completed += size;
    }
    stream.Close();
    std::istringstream input(content);
    std::string line;
    if (!std::getline(input, line) || line != "WOWDECOMP_NATIVE_SHELL 1")
        throw std::runtime_error("Unsupported native shell settings version");
    ShellSettings settings;
    unsigned seen = 0;
    while (std::getline(input, line)) {
        auto space = line.find(' ');
        if (space == std::string::npos)
            throw std::runtime_error("Malformed native shell settings record");
        auto key = line.substr(0, space), value = line.substr(space + 1);
        unsigned bit;
        if (key == "width") {
            bit = 1;
            settings.width = Number<int>(value);
        } else if (key == "height") {
            bit = 2;
            settings.height = Number<int>(value);
        } else if (key == "launches") {
            bit = 4;
            settings.launches = Number<uint64_t>(value);
        } else if (key == "text") {
            bit = 8;
            if (value.size() % 2 || value.size() > 8192)
                throw std::runtime_error("Invalid settings text extent");
            settings.text.clear();
            for (size_t i = 0; i < value.size(); i += 2)
                settings.text.push_back(static_cast<char>(Hex(value[i]) * 16 + Hex(value[i + 1])));
        } else
            throw std::runtime_error("Unknown native shell settings record");
        if (seen & bit)
            throw std::runtime_error("Duplicate native shell settings record");
        seen |= bit;
    }
    if (seen != 15)
        throw std::runtime_error("Missing native shell settings record");
    Validate(settings);
    return settings;
}
void SaveSettings(const File::Native::Paths &paths, const ShellSettings &settings) {
    Validate(settings);
    std::ostringstream content;
    content << "WOWDECOMP_NATIVE_SHELL 1\nwidth " << settings.width << "\nheight " << settings.height
            << "\nlaunches " << settings.launches << "\ntext ";
    content << std::hex << std::setfill('0');
    for (unsigned char c : settings.text)
        content << std::setw(2) << unsigned(c);
    content << '\n';
    auto text = content.str();
    Stream stream;
    std::string temporary;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        uint8_t random[8];
        OsSecureRandom(random, sizeof(random));
        std::ostringstream name;
        name << "native-shell-" << std::hex << std::setfill('0');
        for (auto c : random)
            name << std::setw(2) << unsigned(c);
        name << ".tmp";
        temporary = Path(paths, name.str().c_str());
        if (File::Native::Open(temporary.c_str(), 2, File::Native::Disposition::CreateNew, stream.file))
            break;
        if (SErrGetLastError() != 80 && SErrGetLastError() != 183)
            throw std::runtime_error("Cannot create temporary native shell settings");
    }
    if (!stream.file)
        throw std::runtime_error("Cannot reserve temporary native shell settings");
    struct Remove {
        std::string path;
        Stream &stream;
        ~Remove() {
            if (!path.empty()) {
                if (stream.file) {
                    File::Close(stream.file);
                    stream.file = nullptr;
                }
                File::Delete(path.c_str());
            }
        }
    } cleanup{temporary, stream};
    size_t completed = 0;
    while (completed < text.size()) {
        int32_t size = static_cast<int32_t>(text.size() - completed);
        if (!File::Write(stream.file, text.data() + completed, &size) || size <= 0)
            throw std::runtime_error("Cannot write native shell settings");
        completed += size;
    }
    if (!File::Flush(stream.file))
        throw std::runtime_error("Cannot flush native shell settings");
    stream.Close();
    if (!File::Native::AtomicReplace(temporary.c_str(), Path(paths, "native-shell.settings").c_str()))
        throw std::runtime_error("Cannot replace native shell settings");
    cleanup.path.clear();
}
} // namespace Native
} // namespace Services
