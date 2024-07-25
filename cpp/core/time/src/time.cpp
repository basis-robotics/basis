#include <basis/core/time.h>

#include <chrono>

#include <thread>
#include <time.h>

namespace basis::core {
namespace {
  static std::atomic<bool> using_simulated_time = false;
  static int64_t simulated_time_ns = 0;
}
MonotonicTime MonotonicTime::FromSeconds(double seconds) { return {TimeBase::SecondsToNanoseconds(seconds)}; }

MonotonicTime MonotonicTime::Now() {
  if(using_simulated_time) {
    return {simulated_time_ns};
  }
  timespec ts;
  // not CLOCK_MONOTONIC_RAW because it's unsupported by clock_nanosleep
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return {TimeBase::SecondsToNanoseconds(ts.tv_sec) + ts.tv_nsec};
}

void MonotonicTime::SleepUntil() const {
  timespec ts = ToTimespec();

  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
}

void MonotonicTime::SetSimulatedTime(int64_t nanoseconds) {
  simulated_time_ns = nanoseconds;
  using_simulated_time = true;
}

} // namespace basis::core