// Exercise CM2Cache::Initialize's exact adapted seed statements, without the
// engine/GPU startup dependencies. Capture the argument passed to CRndSeed.
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void require(bool condition) {
    if (!condition) std::abort(); // Remain active in release/NDEBUG builds.
}

static int draws[2];
static unsigned calls;
static int controlledRand() {
    require(calls < 2);
    return draws[calls++];
}

struct CRndSeed {
    uint32_t value;
    explicit CRndSeed(uint32_t input) : value(input) {}
};

static uint32_t cacheSeed() {
#define rand controlledRand
#include "RovNativeM2CacheSeed.inc"
#undef rand
    return seed.value;
}

int main() {
    // Original 15-bit rand range, signed-shift boundary, discarded upper bits,
    // observed failing draw, and Linux's full nonnegative int range.
    const int values[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000,
                          0x10001, 349517445, INT_MAX};
    for (int first : values) {
        for (int second : values) {
            draws[0] = first;
            draws[1] = second;
            calls = 0;
            // Independent wide arithmetic: retain low 16 bits of each draw.
            const uint32_t expected = static_cast<uint32_t>(
                uint64_t(first % 65536) * 65536 + second % 65536);
            require(cacheSeed() == expected);
            require(calls == 2);
        }
    }
    std::puts("m2-cache-seed=passed: 81 boundary pairs, original random bits, two ordered draws");
}
