#include <basis/core/time.h>

#include <chrono>
#include <thread>
#include <time.h>

namespace basis::core {
MonotonicTime MonotonicTime::FromSeconds(double seconds) { return {TimeBase::SecondsToNanoseconds(seconds)}; }

MonotonicTime MonotonicTime::Now() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

  return {TimeBase::SecondsToNanoseconds(ts.tv_sec) + ts.tv_nsec};
}

void MonotonicTime::SleepUntil() const {
  std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(nsecs)));
}

} // namespace basis::core