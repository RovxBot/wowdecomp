#include "Package.h"
#include "ThirdParty/zlib/zlib.h"
#include "ThirdParty/bzip2/bzlib.h"
extern "C" {
#include "ThirdParty/zlib/contrib/blast/blast.h"
#include "ThirdParty/lzma/LzmaDecode.h"
}
#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>

namespace Blizzard { namespace Mopaq { namespace Native {
namespace {
struct BlastBuffer { const Bytes& source; Bytes& output; size_t capacity; bool provided = false; };
unsigned BlastRead(void* cookie, unsigned char** data) {
    auto& buffer = *static_cast<BlastBuffer*>(cookie);
    if (buffer.provided) return 0;
    buffer.provided = true;
    *data = const_cast<unsigned char*>(buffer.source.data());
    return static_cast<unsigned>(buffer.source.size());
}
int BlastWrite(void* cookie, unsigned char* data, unsigned size) {
    auto& buffer = *static_cast<BlastBuffer*>(cookie);
    if (size > buffer.capacity - buffer.output.size()) return 1;
    buffer.output.insert(buffer.output.end(), data, data + size);
    return 0;
}
Bytes Stage(const Bytes& input, size_t capacity, unsigned codec) {
    if (codec == 8) {
        // Bundled blast 1.1 initializes mutable static Huffman tables on its
        // first call (blast.c's `virgin` flag). Multiple native file queues may
        // reach this path concurrently. Keep the dependency unchanged and
        // serialize its calls at this boundary, including initialization.
        static std::mutex blastMutex;
        std::lock_guard<std::mutex> lock(blastMutex);
        Bytes output;
        BlastBuffer buffer{input, output, capacity};
        if (blast(BlastRead, &buffer, BlastWrite, &buffer)) throw Error("MPQ PKWARE decoding failed");
        return output;
    }
    Bytes output(capacity);
    if (codec == 2) {
        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(input.data());
        stream.avail_in = static_cast<uInt>(input.size());
        stream.next_out = output.data(); stream.avail_out = static_cast<uInt>(capacity);
        if (inflateInit(&stream) != Z_OK) throw Error("MPQ zlib initialization failed");
        int status = inflate(&stream, Z_FINISH);
        size_t used = stream.total_out;
        bool complete = status == Z_STREAM_END && !stream.avail_in;
        inflateEnd(&stream);
        if (!complete) throw Error("MPQ zlib decoding failed");
        output.resize(used);
    } else if (codec == 16) {
        unsigned size = static_cast<unsigned>(capacity);
        if (BZ2_bzBuffToBuffDecompress(reinterpret_cast<char*>(output.data()), &size,
                reinterpret_cast<char*>(const_cast<uint8_t*>(input.data())),
                static_cast<unsigned>(input.size()), 0, 0) != BZ_OK) throw Error("MPQ bzip2 decoding failed");
        output.resize(size);
    } else if (codec == 0x12) {
        if (input.size() < 14 || input[0]) throw Error("Invalid MPQ LZMA header");
        CLzmaDecoderState decoder{};
        if (LzmaDecodeProperties(&decoder.Properties, input.data() + 1, 5) != LZMA_RESULT_OK)
            throw Error("Invalid MPQ LZMA properties");
        std::vector<CProb> probabilities(LzmaGetNumProbs(&decoder.Properties));
        decoder.Probs = probabilities.data();
        SizeT consumed = 0, produced = 0;
        if (LzmaDecode(&decoder, input.data() + 14, input.size() - 14, &consumed,
                       output.data(), capacity, &produced) != LZMA_RESULT_OK)
            throw Error("MPQ LZMA decoding failed");
        output.resize(produced);
    } else if (codec == 0x20) {
        if (input.size() < 4) throw Error("Truncated MPQ sparse header");
        size_t length = uint32_t(input[0]) << 24 | uint32_t(input[1]) << 16 | uint32_t(input[2]) << 8 | input[3];
        if (length > capacity) throw Error("MPQ sparse output exceeds capacity");
        output.resize(length);
        size_t pos = 0;
        for (size_t i = 4; i < input.size();) {
            uint8_t control = input[i++];
            size_t count = std::min<size_t>((control & 127) + (control & 128 ? 1 : 3), length - pos);
            if (!count) throw Error("Trailing MPQ sparse data");
            if (control & 128) {
                if (count > input.size() - i) throw Error("Truncated MPQ sparse literal");
                std::memcpy(output.data() + pos, input.data() + i, count); i += count;
            }
            pos += count;
        }
        if (pos != length) throw Error("Short MPQ sparse output");
    } else throw Error("Unsupported MPQ compression method " + std::to_string(codec));
    return output;
}
}
Bytes Decompress(const Bytes& input, size_t capacity, bool implode) {
    if (capacity > 256 * 1024 * 1024 || input.size() > 256 * 1024 * 1024) throw Error("Invalid MPQ decompression extent");
    // IOMpqReadUnit's MPQ_FILE_IMPLODE path always invokes blast, including
    // equal/larger encoded sectors. Only the multi-compression path uses the
    // equal-size raw-sector convention from System_SComp::Decompress.
    if (implode) return Stage(input, capacity, 8);
    if (input.size() > capacity) throw Error("Invalid MPQ multi-compression extent");
    if (input.size() == capacity) return input;
    if (input.empty()) throw Error("Empty compressed MPQ sector");
    unsigned method = input[0];
    Bytes payload(input.begin() + 1, input.end());
    if (method == 0x22 || method == 0x30)
        return Stage(Stage(payload, capacity, method == 0x22 ? 2 : 16), capacity, 0x20);
    return Stage(payload, capacity, method);
}
}}}
