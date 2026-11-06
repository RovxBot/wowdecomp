// Exercise the adapted full client's actual FreeType macros and C entry points.
#include <freetype/internal/ftstream.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void require(bool condition, const char* operation) {
    if (!condition) {
        std::fprintf(stderr, "FreeType stream check failed: %s\n", operation);
        std::exit(1);
    }
}

static FT_ULong readBytes(FT_Stream stream, FT_ULong offset, FT_Byte* buffer, FT_ULong count) {
    if (offset > stream->size || count > stream->size - offset) return 0;
    std::memcpy(buffer, stream->base + offset, count);
    return count;
}

int main() {
    struct Vector { FT_Byte bytes[4]; FT_Long value; };
    const Vector vectors[] = {
        {{0x00, 0x00, 0x00, 0x00}, 0L},
        {{0x00, 0x00, 0x00, 0x01}, 1L},
        {{0x00, 0xFF, 0xFF, 0xFF}, 16777215L},
        {{0x01, 0x02, 0x03, 0x04}, 16909060L},
        {{0x7F, 0xFF, 0xFF, 0xFF}, 2147483647L},
        {{0x80, 0x00, 0x00, 0x00}, (-2147483647L - 1)},
        {{0x80, 0x00, 0x00, 0x01}, -2147483647L},
        {{0xCD, 0x00, 0x00, 0x00}, -855638016L}, // Observed signed high byte -51.
        {{0xCD, 0x12, 0x34, 0x56}, -854444970L},
        {{0xFF, 0x00, 0x00, 0x00}, -16777216L},
        {{0xFF, 0xFF, 0xFF, 0xFE}, -2L},
        {{0xFF, 0xFF, 0xFF, 0xFF}, -1L},
    };
    for (const auto& vector : vectors) {
        for (unsigned offset = 0; offset < 4; ++offset) {
            // Extra backing storage keeps truncated-frame pointer preflights
            // inside an array; only the four advertised bytes are available.
            alignas(4) FT_Byte storage[12] = {};
            FT_Byte* const begin = storage + offset;
            std::memcpy(begin, vector.bytes, 4);
            FT_Byte* p = begin;
            require(NEXT_Long(p) == vector.value && p == begin + 4, "NEXT_Long value/advance");
            p = begin;
            // Retain the donor's unsigned-long cast, including LP64 widening
            // of a negative FT_Long. This fix changes no unsigned wrapper.
            const FT_ULong unsignedValue = static_cast<FT_ULong>(vector.value);
            require(NEXT_ULong(p) == unsignedValue && p == begin + 4, "NEXT_ULong value/advance");

            struct FT_StreamRec_ record = {};
            FT_Stream stream = &record; // GET_* macros use this name.
            record.cursor = begin;
            record.limit = begin + 4;
            require(FT_Get_Long(stream) == vector.value && record.cursor == begin + 4, "FT_Get_Long value/advance");
            record.cursor = begin;
            require(GET_ULong() == unsignedValue && record.cursor == begin + 4, "GET_ULong value/advance");
            record.cursor = begin;
            require(GET_Tag4() == unsignedValue && record.cursor == begin + 4, "GET_Tag4 value/advance");

            record.base = storage;
            record.size = offset + 4;
            for (int callback = 0; callback < 2; ++callback) {
                record.read = callback ? readBytes : nullptr;
                record.pos = offset;
                FT_Error error = -1;
                require(FT_Read_Long(stream, &error) == vector.value && !error && record.pos == offset + 4,
                        "FT_Read_Long memory/callback value/advance");
            }
            for (unsigned available = 0; available < 4; ++available) {
                record.cursor = begin;
                record.limit = begin + available;
                require(FT_Get_Long(stream) == 0 && record.cursor == begin, "truncated frame unchanged");
                record.size = offset + available;
                for (int callback = 0; callback < 2; ++callback) {
                    record.read = callback ? readBytes : nullptr;
                    record.pos = offset;
                    FT_Error error = 0;
                    require(FT_Read_Long(stream, &error) == 0 && error == FT_Err_Invalid_Stream_Operation &&
                            record.pos == offset, "truncated memory/callback read unchanged");
                }
            }
        }
    }
    std::puts("freetype-stream=passed: signed BE boundaries, -51 high byte, unsigned casts, alignment, cursors, truncation");
}
