#include <basis/core/time.h>

#include <chrono>

#include <thread>
#include <time.h>

namespace basis::core {
MonotonicTime MonotonicTime::FromSeconds(double seconds) { return {TimeBase::SecondsToNanoseconds(seconds)}; }

MonotonicTime MonotonicTime::Now() {
  timespec ts;
  // not CLOCK_MONOTONIC_RAW because it's unsupported by clock_nanosleep
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return {TimeBase::SecondsToNanoseconds(ts.tv_sec) + ts.tv_nsec};
}

void MonotonicTime::SleepUntil() const {
  timespec ts = ToTimespec();

  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
}

} // namespace basis::core