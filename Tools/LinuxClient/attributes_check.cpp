#include "RovPackedAttributes.h"
#include <cstdio>
#include <limits>

int main() {
    // Odd CRC count puts the following file time on a four-byte boundary.
    alignas(8) const uint8_t packed[] = {1, 2, 3, 4, 8, 7, 6, 5, 4, 3, 2, 1};
    if (RovNative::PackedAttribute<uint32_t>(packed) != 0x04030201u ||
        RovNative::PackedAttribute<uint64_t>(packed + 4) != UINT64_C(0x0102030405060708)) return 1;
    for (uint32_t flags = 0; flags < 8; ++flags) {
        const auto expected = static_cast<uint32_t>(RovNative::AttributeBytes(3, flags) + 8);
        if (!RovNative::ValidAttributeSize(3, expected, flags) ||
            RovNative::ValidAttributeSize(3, expected - 1, flags) ||
            RovNative::ValidAttributeSize(3, expected + 1, flags)) return 2;
    }
    if (RovNative::AttributeBytes(std::numeric_limits<uint32_t>::max(), 7) != UINT64_C(120259084260) ||
        RovNative::ValidAttributeSize(0x20000000u, 8, 2) ||
        !RovNative::ValidAttributeSize(0, 8, 7) ||
        !RovNative::ValidAttributeSize(3, 100, 8) ||
        RovNative::ValidAttributeSize(3, 7, 8)) return 3;
    std::puts("packed-mpq-attributes=passed: unaligned values, all known flags, truncation, wide counts, unknown extension");
}
