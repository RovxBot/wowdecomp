#include "Engine/Source/BlizzardCore/Source/System/Time/Time.h"
#include <chrono>
#include <ctime>
#include <limits>

namespace {
constexpr int64_t epoch = 946684800; // Blizzard Timestamp: nanoseconds since 2000-01-01 UTC.
constexpr int64_t invalid = std::numeric_limits<int64_t>::min();
constexpr int64_t maximum = std::numeric_limits<int64_t>::max();
}
int64_t Blizzard::Time::Milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
Blizzard::Time::Timestamp Blizzard::Time::GetTimestamp() {
    struct Anchor {
        int64_t utc = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() - epoch * 1000000000;
        std::chrono::steady_clock::time_point monotonic = std::chrono::steady_clock::now();
    };
    static const Anchor anchor;
    return anchor.utc + std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - anchor.monotonic).count();
}
Blizzard::Time::Timestamp Blizzard::Time::FromFileTime(uint64_t fileTime) {
    if (fileTime <= 0x0077A5D6AA8BF851) return invalid;
    if (fileTime <= 0x030701FFA04E87AE)
        return static_cast<int64_t>(fileTime * 100 + 0x5143382561530000ULL);
    return maximum;
}
void Blizzard::Time::BreakTime(Timestamp value, TimeRec& output) {
    int64_t seconds = value / 1000000000;
    int32_t nanos = static_cast<int32_t>(value % 1000000000);
    if (nanos < 0) { nanos += 1000000000; --seconds; }
    std::time_t unixTime = static_cast<std::time_t>(seconds + epoch);
    std::tm utc{};
#if defined(_WIN32)
    if (gmtime_s(&utc, &unixTime)) { output = {}; return; }
#else
    if (!gmtime_r(&unixTime, &utc)) { output = {}; return; }
#endif
    output = {utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
              utc.tm_min, utc.tm_sec, nanos, utc.tm_wday, utc.tm_yday};
}
Blizzard::Time::Timestamp Blizzard::Time::MakeTime(const TimeRec& value) {
    if (value.year < 1601 || value.year > 9999 || value.month < 1 || value.month > 12 ||
        value.day < 1 || value.day > 31 || value.hour < 0 || value.hour > 23 ||
        value.minute < 0 || value.minute > 59 || value.second < 0 || value.second > 59 ||
        value.nanosecond < 0 || value.nanosecond >= 1000000000) return invalid;
    std::tm utc{};
    utc.tm_year = value.year - 1900; utc.tm_mon = value.month - 1; utc.tm_mday = value.day;
    utc.tm_hour = value.hour; utc.tm_min = value.minute; utc.tm_sec = value.second;
#if defined(_WIN32)
    auto seconds = static_cast<int64_t>(_mkgmtime64(&utc)) - epoch;
#else
    auto seconds = static_cast<int64_t>(timegm(&utc)) - epoch;
#endif
    if (utc.tm_year != value.year - 1900 || utc.tm_mon != value.month - 1 || utc.tm_mday != value.day)
        return invalid;
    if (seconds <= invalid / 1000000000) return invalid;
    if (seconds > maximum / 1000000000) return maximum;
    int64_t nanoseconds = seconds * 1000000000;
    if (nanoseconds > maximum - value.nanosecond) return maximum;
    return nanoseconds + value.nanosecond;
}
