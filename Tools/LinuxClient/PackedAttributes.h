#pragma once
#include <cstdint>
#include <cstring>

namespace RovNative {
// MPQ attributes are packed little-endian data, not aligned runtime objects.
template <typename T> T PackedAttribute(const uint8_t* bytes) {
    T value;
    std::memcpy(&value, bytes, sizeof(value));
    return value; // The full Linux target currently requires little-endian x86-64.
}
constexpr uint64_t AttributeBytes(uint32_t count, uint32_t flags) {
    return uint64_t(count) * ((flags & 1 ? 4 : 0) + (flags & 2 ? 8 : 0) + (flags & 4 ? 16 : 0));
}
constexpr bool ValidAttributeSize(uint32_t count, uint32_t size, uint32_t flags) {
    return size >= 8 && uint64_t(size) <= uint64_t(count) * 512 + 8 &&
           uint64_t(size) - 8 >= AttributeBytes(count, flags) &&
           ((flags & ~7u) || uint64_t(size) - 8 == AttributeBytes(count, flags));
}
}
