#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <ratio>
#include <sys/time.h>
#include <utility>

/**
 * Time
 *
 * Some design notes:
 *   Floating point time can suffer from precision issues after roundtripping through text or when doing math.
 *   By default, we shouldn't lose _any_ precision.
 *
 *   Beware of the temptation to use realtime or wall time. I've been in multiple situations where a robot was forced to
 * acquire a PTP lock before operating, to avoid a later clock jump. There are a few places where it is valuable to
 * actually use wall time:
 *      1. Data storage. It's highly recommended to continually store the difference between monotonic and real time,
 * and then when ingesting or replaying data, use the last record as an estimation of the true time.
 *      2. Interfacing with external systems. No recommendations here, yet.
 *
 *   Simulated time:
 *      It's recommended to avoid calling MonotonicTime::Now() inside callbacks, for now.
 * Simulated time will be different per callback - there will be an attempt to handle this, but it will be safer
 * not to.
 */
namespace basis::core {
namespace time {
  static constexpr int64_t INVALID_NSECS = std::numeric_limits<int64_t>::min();
}

/**
 * Base of all time types. Don't construct this directly.
 *
 * Has nanosecond precision.
 */
struct TimeBase {
protected:

  TimeBase() {}

  TimeBase(int64_t nsecs) : nsecs(nsecs) {}

  static int64_t SecondsToNanoseconds(double seconds) { return seconds * NSECS_IN_SECS; }

  constexpr static int64_t NSECS_IN_SECS = std::nano::den;
public:
  // TODO: ensure this doesn't allow comparison between duration and timepoint
  auto operator<=>(const TimeBase&) const = default;

  int64_t nsecs = time::INVALID_NSECS;

  bool IsValid() const { return nsecs != time::INVALID_NSECS; }

  double ToSeconds() const { return (nsecs / NSECS_IN_SECS) + double(nsecs % NSECS_IN_SECS) / NSECS_IN_SECS; } // NOLINT(bugprone-integer-division) - intentional use of double/int mix

  timeval ToTimeval() const { return {.tv_sec = nsecs / NSECS_IN_SECS, .tv_usec = 1000 * (nsecs % NSECS_IN_SECS)}; }
  timespec ToTimespec() const { return {.tv_sec = nsecs / NSECS_IN_SECS, .tv_nsec = (nsecs % NSECS_IN_SECS)}; }
};

#ifndef IGNORE_YEAR_2038
// I'd like to think this software will be alive in some form in 14 years.
static_assert(!std::is_same<time_t, int32_t>::value, "This platform is likely to hit the year 2038 problem.");
#endif

struct Duration : public TimeBase {
  auto operator<=>(const Duration&) const = default;

  static Duration FromSecondsNanoseconds(int64_t seconds, int64_t nanoseconds) {
    return {TimeBase::SecondsToNanoseconds(seconds) + nanoseconds};
  }
  static Duration FromSeconds(double seconds) { return {TimeBase::SecondsToNanoseconds(seconds)}; }


  std::pair<int32_t, int32_t> ToRosDuration() { return {nsecs / NSECS_IN_SECS, nsecs % NSECS_IN_SECS}; }

protected:
  using TimeBase::TimeBase;
};

struct TimePoint : public TimeBase {
  auto operator<=>(const TimePoint&) const = default;

  std::pair<uint32_t, uint32_t> ToRosTime() { return {nsecs / NSECS_IN_SECS, nsecs % NSECS_IN_SECS}; }

protected:
  using TimeBase::TimeBase;
};

// TODO: do we need "RealTimeDuration???"
struct RealTimeDuration : public Duration {
  static RealTimeDuration FromSeconds(double seconds) { return {TimeBase::SecondsToNanoseconds(seconds)}; }

protected:
  using Duration::Duration;
};

/**
 * Monotonic time - used as the basis for all robot time operations.
 */
struct MonotonicTime : public TimePoint {
  static MonotonicTime FromNanoseconds(int64_t ns);

  static MonotonicTime FromSeconds(double seconds);

  static MonotonicTime Now(bool ignore_simulated_time = false);

  MonotonicTime &operator+=(const Duration &duration) {
    nsecs += duration.nsecs;
    return *this;
  }

  MonotonicTime operator+(const Duration &duration) const {
    MonotonicTime out(nsecs + duration.nsecs);
    return out;
  }

  Duration operator-(const MonotonicTime &other) const {
    Duration out;
    out.nsecs = nsecs - other.nsecs;
    return out;
  }

  void SleepUntil(uint64_t run_token) const;

  static void SetSimulatedTime(int64_t nanoseconds, uint64_t run_token);

  static bool UsingSimulatedTime();

  static uint64_t GetRunToken();
protected:
  using TimePoint::TimePoint;
};

#if 0


/**
 *  WallTime - this class should be used when it's desired to store an actual real time.
 */
struct WallTime {
    MonotonicTime base_time;
    uint64_t monotonic_epoch;
};

#endif
} // namespace basis::core