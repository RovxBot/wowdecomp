// The three scalar conversions needed by original Gxu. Source: product 12340
// Tempest/CMath.cpp, CeilToInt64/fuint_pi/fint_n (0x00493B60/0x00493B90/0x005773B0).
// Preserve the original float expressions, including their epsilon. Validate
// before integer conversion; this native adaptation makes no new VERIFY claim.
#include "Engine/Source/Tempest/CMath.h"
#include <stdexcept>
namespace {
float Bounded(float value, double low, double high) {
    if (!std::isfinite(value) || double(value) < low || double(value) >= high)
        throw std::out_of_range("Native Gxu pixel conversion outside integer bounds");
    return value;
}
}
uint32_t CeilToInt64(float value) {
    return static_cast<uint32_t>(static_cast<int64_t>(
        Bounded(value + 0.99995f, -9223372036854775808.0, 9223372036854775808.0)));
}
uint32_t CMath::fuint_pi(float f) {
    if (f > 0.0f) return static_cast<uint32_t>(Bounded(f + 0.99995f, 0, 4294967296.0));
    return static_cast<uint32_t>(static_cast<int32_t>(Bounded(f, -2147483648.0, 2147483648.0)));
}
int32_t CMath::fint_n(float f) {
    if (f > 0.0f) return static_cast<int32_t>(Bounded(f + 0.5f, 0, 2147483648.0));
    return static_cast<int32_t>(Bounded(f - 0.5f, -2147483648.0, 2147483648.0));
}
