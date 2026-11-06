// Exercise the adapted full client's actual FreeType macros and C entry points.
#include <freetype/internal/ftstream.h>
#include <cstddef>
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

static void checkShorts() {
    struct Fields {
        FT_Short signed16;
        FT_UShort unsigned16;
        FT_Int32 signed32;
        FT_UInt32 unsigned32;
    };
#define FT_STRUCTURE Fields
    static const FT_Frame_Field bigEndian[] = {
        FT_FRAME_START(8),
        FT_FRAME_SHORT(signed16), FT_FRAME_USHORT(unsigned16),
        FT_FRAME_SHORT(signed32), FT_FRAME_USHORT(unsigned32),
        FT_FRAME_END
    };
    static const FT_Frame_Field littleEndian[] = {
        FT_FRAME_START(8),
        FT_FRAME_SHORT_LE(signed16), FT_FRAME_USHORT_LE(unsigned16),
        FT_FRAME_SHORT_LE(signed32), FT_FRAME_USHORT_LE(unsigned32),
        FT_FRAME_END
    };
#undef FT_STRUCTURE
    const unsigned values[] = {0, 1, 0xFF, 0x100, 0x1234, 0x7FFF,
                               0x8000, 0x8001, 0xFF00, 0xFFFE, 0xFFFF};
    for (unsigned value : values) {
        const int signedValue = value < 0x8000 ? static_cast<int>(value) : static_cast<int>(value) - 0x10000;
        for (int little = 0; little < 2; ++little) {
            for (unsigned offset = 0; offset < 4; ++offset) {
                // Backing storage also bounds pointer arithmetic in truncated
                // reads; advertised size/limit still exposes only valid bytes.
                alignas(4) FT_Byte storage[16] = {};
                FT_Byte* const begin = storage + offset;
                for (unsigned pair = 0; pair < 4; ++pair) {
                    begin[pair * 2] = static_cast<FT_Byte>(little ? value : value >> 8);
                    begin[pair * 2 + 1] = static_cast<FT_Byte>(little ? value >> 8 : value);
                }
                FT_Byte* p = begin;
                const int decoded = little ? NEXT_ShortLE(p) : NEXT_Short(p);
                require(decoded == signedValue && p == begin + 2, "NEXT_Short BE/LE value/advance");
                p = begin;
                const unsigned unsignedDecoded = little ? NEXT_UShortLE(p) : NEXT_UShort(p);
                require(unsignedDecoded == value && p == begin + 2, "NEXT_UShort BE/LE value/advance");

                struct FT_StreamRec_ record = {};
                FT_Stream stream = &record;
                record.cursor = begin;
                record.limit = begin + 2;
                const FT_Short got = little ? FT_Get_ShortLE(stream) : FT_Get_Short(stream);
                require(got == signedValue && record.cursor == begin + 2, "FT_Get_Short BE/LE value/advance");
                record.cursor = begin;
                const FT_UShort unsignedGot = little ? GET_UShortLE() : GET_UShort();
                require(unsignedGot == value && record.cursor == begin + 2, "GET_UShort BE/LE value/advance");
                record.base = storage;
                record.size = offset + 2;
                for (int callback = 0; callback < 2; ++callback) {
                    record.read = callback ? readBytes : nullptr;
                    record.pos = offset;
                    FT_Error error = -1;
                    const FT_Short read = little ? FT_Read_ShortLE(stream, &error) : FT_Read_Short(stream, &error);
                    require(read == signedValue && !error && record.pos == offset + 2,
                            "FT_Read_Short BE/LE memory/callback value/advance");
                }
                for (unsigned available = 0; available < 2; ++available) {
                    record.cursor = begin;
                    record.limit = begin + available;
                    const FT_Short truncated = little ? FT_Get_ShortLE(stream) : FT_Get_Short(stream);
                    require(truncated == 0 && record.cursor == begin, "truncated short frame unchanged");
                    record.size = offset + available;
                    for (int callback = 0; callback < 2; ++callback) {
                        record.read = callback ? readBytes : nullptr;
                        record.pos = offset;
                        FT_Error error = 0;
                        const FT_Short read = little ? FT_Read_ShortLE(stream, &error) : FT_Read_Short(stream, &error);
                        require(read == 0 && error == FT_Err_Invalid_Stream_Operation && record.pos == offset,
                                "truncated short BE/LE memory/callback unchanged");
                    }
                }

                // Use the actual C FT_Read_Fields dispatch reached by the font
                // header, including signed extension into wider destinations.
                record.read = nullptr;
                record.size = offset + 8;
                const FT_Frame_Field* fields = little ? littleEndian : bigEndian;
                for (int ownFrame = 0; ownFrame < 2; ++ownFrame) {
                    record.pos = offset;
                    record.cursor = ownFrame ? nullptr : begin;
                    record.limit = ownFrame ? nullptr : begin + 8;
                    Fields result = {};
                    require(FT_Read_Fields(stream, fields + (ownFrame ? 0 : 1), &result) == FT_Err_Ok,
                            "FT_Read_Fields BE/LE succeeds");
                    require(result.signed16 == signedValue && result.unsigned16 == value &&
                            result.signed32 == signedValue && result.unsigned32 == value,
                            "FT_Read_Fields BE/LE signed/unsigned widths");
                    require(record.pos == offset + (ownFrame ? 8 : 0) &&
                            record.cursor == (ownFrame ? nullptr : begin + 8) &&
                            record.limit == (ownFrame ? nullptr : begin + 8),
                            "FT_Read_Fields owned/existing frame cursors");
                }
                for (unsigned available = 0; available < 8; ++available) {
                    record.pos = offset;
                    record.size = offset + available;
                    record.cursor = record.limit = nullptr;
                    Fields result = {123, 456, 789, 1234};
                    require(FT_Read_Fields(stream, fields, &result) == FT_Err_Invalid_Stream_Operation &&
                            result.signed16 == 123 && result.unsigned16 == 456 &&
                            result.signed32 == 789 && result.unsigned32 == 1234 &&
                            record.pos == offset && !record.cursor && !record.limit,
                            "FT_Read_Fields truncated frame leaves output/cursors unchanged");
                }
            }
        }
    }
}

int main() {
    checkShorts();
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
    std::puts("freetype-stream=passed: signed/unsigned BE/LE shorts, FT_Read_Fields, BE longs/-51, alignment, cursors, truncation");
}
