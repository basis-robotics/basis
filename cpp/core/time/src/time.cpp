#include <basis/core/time.h>

#include <cstdint>
#include <time.h>

#include <iostream>

#include <atomic>
namespace basis::core {
namespace {
static std::atomic<int64_t> simulated_time_ns = time::INVALID_NSECS;
static std::atomic<uint64_t> current_run_token = 0;
} // namespace
MonotonicTime MonotonicTime::FromNanoseconds(int64_t ns) {
  return {ns};
}

MonotonicTime MonotonicTime::FromSeconds(double seconds) { return {TimeBase::SecondsToNanoseconds(seconds)}; }

MonotonicTime MonotonicTime::Now(bool ignore_simulated_time) {
  if (!ignore_simulated_time && UsingSimulatedTime()) {
    return {simulated_time_ns};
  }
  timespec ts;
  // not CLOCK_MONOTONIC_RAW because it's unsupported by clock_nanosleep
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return {TimeBase::SecondsToNanoseconds(ts.tv_sec) + ts.tv_nsec};
}

void MonotonicTime::SleepUntil(uint64_t run_token) const {
  // TODO: these sleeps are both inaccurate and not performant if under sim time. Use a condition variable!
  if (UsingSimulatedTime()) {
    while(Now().nsecs < nsecs && run_token == current_run_token) {
      // Spin until time catches up
      timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000; // .1 ms
      clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
    }
  }
  else {
    timespec ts = ToTimespec();

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
  }
}

void MonotonicTime::SetSimulatedTime(int64_t nanoseconds, uint64_t run_token) {
  simulated_time_ns = nanoseconds;
  current_run_token = run_token;
}

bool MonotonicTime::UsingSimulatedTime() {
  return simulated_time_ns != time::INVALID_NSECS;
}

uint64_t MonotonicTime::GetRunToken() {
  return current_run_token;
}

} // namespace basis::core