// Original 12340 scalar model/FFX math, with native conversion preflight.
#include "Engine/Source/Tempest/CMath.h"
#include <stdexcept>

// OFFSET: 0x00407930
int32_t FloatToInt64(float value) {
    if (!std::isfinite(value) || (value > 0 ? double(value) >= 9223372036854775808.0 : double(value) < INT32_MIN))
        throw std::out_of_range("Native model time/coordinate conversion");
    if (value > 0) return static_cast<int32_t>(static_cast<int64_t>(value));
    return static_cast<int32_t>(value);
}
// OFFSET: 0x006E9320
bool CMath::fnotequal(float a, float b) {
    return !(fabsf(a - b) < 0.00000095367432f);
}
// OFFSET: 0x005FE800
void CMath::split_(float x, float* frac, int32_t* whole) {
    if (!std::isfinite(x) || double(x) <= INT32_MIN || double(x) >= 2147483648.0)
        throw std::out_of_range("Native model angle conversion");
    if (x > 0) *whole = static_cast<int32_t>(static_cast<int64_t>(x));
    else *whole = static_cast<int32_t>(x) - 1;
    *frac = x - *whole;
}
// OFFSET: 0x006F7A10
float CMath::sin_(float x) {
    float frac;
    int32_t whole;
    CMath::split_(x * 0.31830987f - 0.5f, &frac, &whole);
    float result = 1.0f - frac * ((6.0f - 4.0f * frac) * frac);
    return (whole & 1) ? -result : result;
}
