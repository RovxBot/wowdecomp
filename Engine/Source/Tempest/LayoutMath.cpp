// Original 12340 scalar layout comparisons, separated from CMath.cpp for native selection.
// Historical verification annotations remain attached to unchanged bodies.
#include <cmath>
#include "Engine/Source/Tempest/CMath.h"

// VERIFY: 2ff13d8c auto
// OFFSET: 0x00482870
int32_t fequalz(float a, float b, float z) {
    return fabsf(a - b) < z;
}

// VERIFY: 334b4c2a manual
// OFFSET: 0x00482900
bool fnotequal(float a, float b) {
    return !(fabsf(a - b) < 0.00000023841858f);
}

