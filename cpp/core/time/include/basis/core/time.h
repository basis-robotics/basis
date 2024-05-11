#pragma once

#include <cmath>
#include <ratio>
#include <cstdint>
#include <limits>
#include <sys/time.h>
#include <utility>

/**
 * Time
 * 
 * Some design notes:
 *   Floating point time can suffer from precision issues after roundtripping through text or when doing math.
 *   By default, we shouldn't lose _any_ precision.
 *
 *   Beware of the temptation to use realtime or wall time. I've been in multiple situations where a robot was forced to acquire a PTP lock
 *   before operating, to avoid a later clock jump.
 *   There are a few places where it is valuable to actually use wall time:
 *      1. Data storage. It's highly recommended to continually store the difference between monotonic and real time, and then when ingesting
 *         or replaying data, use the last record as an estimation of the true time.
 *      2. Interfacing with external systems. No recommendations here, yet. 
 * 
 *   Simulated time:
 *      Will not be initially implemented. It's recommended to avoid calling MonotonicTime::Now() inside callbacks, for now.
 *      Simulated time will be different per callback - there will be an attempt to handle this, but it will be safer not to 
 *      This can eventually be worked around with 
 */
namespace basis::core {

/**
 * Base of all time types. Don't construct this directly.
 * 
 * Has nanosecond precision. Doesn't 
 */
struct TimeBase {
protected:
    TimeBase() {

    }

    TimeBase(int64_t nsecs) : nsecs(nsecs) {

    }

    static int64_t SecondsToNanoseconds(double seconds) {
        return seconds * NSECS_IN_SECS;
    }

    /* todo
    TimeBase(double seconds) {

    }
    */
    constexpr static int64_t NSECS_IN_SECS = std::nano::den;

public:
    int64_t nsecs = std::numeric_limits<int64_t>::min();

    bool IsValid() {
        return nsecs != std::numeric_limits<int64_t>::min();
    }

    double ToSeconds() {
        return (nsecs / NSECS_IN_SECS) + double(nsecs % NSECS_IN_SECS) / NSECS_IN_SECS;
    }

    timeval ToTimeval() {
        return {.tv_sec=nsecs / NSECS_IN_SECS, .tv_usec=1000*(nsecs % NSECS_IN_SECS)};
    }
};

static_assert(!std::is_same<time_t, int32_t>::value, "This platform is likely to hit the year 2038 problem.");

struct Duration : public TimeBase {
    std::pair<int32_t, int32_t> ToRosDuration() {
        return {nsecs / NSECS_IN_SECS, nsecs % NSECS_IN_SECS};
    }

protected:
    using TimeBase::TimeBase;
};

struct TimePoint : public TimeBase {
    std::pair<uint32_t, uint32_t> ToRosTime() {
        return {nsecs / NSECS_IN_SECS, nsecs % NSECS_IN_SECS};
    }
protected:
    using TimeBase::TimeBase;
};

struct RealTimeDuration : public Duration {
    static RealTimeDuration FromSeconds(double seconds) {
        return {TimeBase::SecondsToNanoseconds(seconds)};
    }
protected:
    using Duration::Duration;
};


/**
 * Monotonic time - used as the basis for all robot time operations.
 */
struct MonotonicTime : public TimePoint {
    static MonotonicTime FromSeconds(double seconds) {
        return {TimeBase::SecondsToNanoseconds(seconds)};
    }
protected:
    using TimePoint::TimePoint;

#if 0

    static double ToUnixWallTime() {

    }
#endif

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
}