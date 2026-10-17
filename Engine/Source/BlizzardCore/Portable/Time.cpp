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
namespace {
bool Leap(int32_t year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
int64_t DaysBeforeYear(int32_t year) {
    int64_t prior = year - 1;
    return prior * 365 + prior / 4 - prior / 100 + prior / 400;
}
constexpr int32_t daysBeforeMonth[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
}
void Blizzard::Time::BreakTime(Timestamp value, TimeRec& output) {
    int64_t seconds = value / 1000000000;
    int32_t nanos = static_cast<int32_t>(value % 1000000000);
    if (nanos < 0) { nanos += 1000000000; --seconds; }
    int64_t days = seconds / 86400;
    int32_t daySeconds = static_cast<int32_t>(seconds % 86400);
    if (daySeconds < 0) { daySeconds += 86400; --days; }
    int32_t weekday = static_cast<int32_t>((days + 6) % 7);
    if (weekday < 0) weekday += 7;
    int32_t year = 2000;
    while (days < 0) { --year; days += Leap(year) ? 366 : 365; }
    while (days >= (Leap(year) ? 366 : 365)) { days -= Leap(year) ? 366 : 365; ++year; }
    int32_t yearday = static_cast<int32_t>(days);
    int32_t month = 1;
    while (month < 12 && days >= daysBeforeMonth[month] + (month >= 2 && Leap(year) ? 1 : 0)) ++month;
    int32_t day = static_cast<int32_t>(days) - daysBeforeMonth[month - 1] - (month > 2 && Leap(year) ? 1 : 0) + 1;
    output = {year, month, day, daySeconds / 3600, daySeconds / 60 % 60,
              daySeconds % 60, nanos, weekday, yearday};
}
Blizzard::Time::Timestamp Blizzard::Time::MakeTime(const TimeRec& value) {
    if (value.year < 1601 || value.year > 9999 || value.month < 1 || value.month > 12 ||
        value.day < 1 || value.day > 31 || value.hour < 0 || value.hour > 23 ||
        value.minute < 0 || value.minute > 59 || value.second < 0 || value.second > 59 ||
        value.nanosecond < 0 || value.nanosecond >= 1000000000) return invalid;
    const int32_t monthDays = daysBeforeMonth[value.month] - daysBeforeMonth[value.month - 1]
                            + (value.month == 2 && Leap(value.year) ? 1 : 0);
    if (value.day > monthDays) return invalid;
    const int64_t days = DaysBeforeYear(value.year) - DaysBeforeYear(2000)
                       + daysBeforeMonth[value.month - 1] + value.day - 1
                       + (value.month > 2 && Leap(value.year) ? 1 : 0);
    const int64_t seconds = days * 86400 + value.hour * 3600 + value.minute * 60 + value.second;
    if (seconds < 0) {
        uint64_t magnitude = static_cast<uint64_t>(-seconds) * 1000000000 - value.nanosecond;
        if (magnitude >= (uint64_t(1) << 63)) return invalid;
        return -static_cast<int64_t>(magnitude);
    }
    if (seconds > maximum / 1000000000) return maximum;
    int64_t nanoseconds = seconds * 1000000000;
    if (nanoseconds > maximum - value.nanosecond) return maximum;
    return nanoseconds + value.nanosecond;
}
