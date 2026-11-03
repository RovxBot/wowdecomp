#include "fmod_3d.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstdlib>

static void Check(bool condition) { if (!condition) std::abort(); }
int main() {
    Check(FMOD_Cart2Angle(0, 0) == 0);
    Check(FMOD_Cart2Angle(0, 256) == 0);
    Check(FMOD_Cart2Angle(256, 256) == 45);
    Check(FMOD_Cart2Angle(256, 0) == 90);
    Check(FMOD_Cart2Angle(256, -256) == 135);
    Check(FMOD_Cart2Angle(0, -256) == 180);
    Check(FMOD_Cart2Angle(-256, -256) == 225);
    Check(FMOD_Cart2Angle(-256, 0) == 270);
    Check(FMOD_Cart2Angle(-256, 256) == 315);
    for (int x = -256; x <= 256; x += 8) {
        for (int y = -256; y <= 256; y += 8) {
            if (!x && !y) continue;
            double reference = std::atan2(double(y), double(x)) * 180 / std::acos(-1.0);
            if (reference < 0) reference += 360;
            double error = std::fabs(FMOD_Cart2Angle(y, x) - reference);
            Check(std::min(error, 360 - error) < 6.0);
        }
    }
    for (int x : {INT_MIN, -1, 0, 1, INT_MAX})
        for (int y : {INT_MIN, -1, 0, 1, INT_MAX}) {
            int angle = FMOD_Cart2Angle(y, x);
            Check(angle >= 0 && angle < 360);
        }
    std::puts("native-audio-angle=passed: eight directions, independent atan2 grid and full int bounds");
}
