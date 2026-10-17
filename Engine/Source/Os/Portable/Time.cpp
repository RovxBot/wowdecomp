#include "Engine/Source/Os/Time.h"
#include <chrono>
#include <ctime>
#include <thread>

namespace {
OsTimeManager& Manager() { static OsTimeManager manager(OsTimeManager::BestAvailable); return manager; }
int64_t Clocks() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
OsTimeManager::OsTimeManager(TimingMethod method) {
    m_timingTestError = 0;
    m_performanceFrequency = 1000000000;
    m_scaleToMs = 0.000001;
    m_timeBegin = 0;
    m_timingMethod = method == SystemMethod1 ? SystemMethod1 : SystemMethod2;
}
OsTimeManager::TimingMethod OsTimeManager::Calibrate() { return SystemMethod2; }
uint64_t OsTimeManager::Snapshot() { return static_cast<uint64_t>(Clocks() / 1000000); }
uint32_t OsTimeGetTestError() { return Manager().m_timingTestError; }
OsTimeManager::TimingMethod OsTimeGetTimingMethod() { return Manager().m_timingMethod; }
const char* GetTimingMethodName(OsTimeManager::TimingMethod method) {
    switch (method) {
    case OsTimeManager::NotSet: return "[Not Set]";
    case OsTimeManager::BestAvailable: return "[Best Available]";
    case OsTimeManager::SystemMethod1: return "steady_clock milliseconds";
    case OsTimeManager::SystemMethod2: return "steady_clock nanoseconds";
    default: return "[Unknown]";
    }
}
uint32_t OsGetAsyncTimeMs() { return static_cast<uint32_t>(Manager().Snapshot()); }
uint64_t OsGetAsyncTimeClocks() {
    return Manager().m_timingMethod == OsTimeManager::SystemMethod1 ? Clocks() / 1000000 : Clocks();
}
int64_t OsGetAsyncClocksPerSecond() {
    return Manager().m_timingMethod == OsTimeManager::SystemMethod1 ? 1000 : 1000000000;
}
// Timing selection and shutdown are entry-thread lifecycle operations, with all
// readers stopped. The monotonic epoch remains stable across reinitialization.
void OsTimeManagerCreate(OsTimeManager::TimingMethod method) { Manager() = OsTimeManager(method); }
void OsTimeManagerDestroy() { /* No allocated timer state or OS timer-period change. */ }
void OsSleep(uint32_t milliseconds) {
    if (!milliseconds) std::this_thread::yield();
    else std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
void OsGetTimeStamp(char* buffer, uint32_t size) {
    if (!buffer || !size) return;
    buffer[0] = 0;
    std::time_t now = std::time(nullptr);
    std::tm local;
#if defined(_WIN32)
    if (localtime_s(&local, &now)) return;
#else
    if (!localtime_r(&now, &local)) return;
#endif
    if (!std::strftime(buffer, size, "%m%d%y_%H%M%S", &local)) buffer[0] = 0;
}
int32_t OsGetTimeZone() {
    std::time_t now = std::time(nullptr);
    std::tm local;
#if defined(_WIN32)
    if (localtime_s(&local, &now)) return 0;
    long seconds;
    return _get_timezone(&seconds) ? 0 : static_cast<int32_t>(-seconds / 60);
#else
    if (!localtime_r(&now, &local)) return 0;
    // Windows Bias excludes daylight saving; preserve that contract.
    const auto offset = local.tm_gmtoff;
    local.tm_isdst = 0;
    const auto standard = std::mktime(&local);
    return static_cast<int32_t>((offset + std::difftime(now, standard)) / 60);
#endif
}
