#pragma once
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
 * Monotonic time - used as the basis for all robot time operations.
 */
struct MonotonicTime {
    uint64_t nsec = 0;
#if 0
    static Time FromSecsNsecs(uint32_t secs, uint32_t nsecs) {

    }

    static std::pair<uint32_t, uint32_t> ToSecsNsecs() {

    }

    static Time FromFloatSeconds(double seconds) {

    }

    static double ToUnixWallTime() {

    }
#endif
};
#if 0
struct RealtimeOffset {
    uint64_t nsec = 0;
};

/**
 *  WallTime - this class should be used when it's desired to store an actual real time.
 */
struct WallTime {
    MonotonicTime base_time;
    uint64_t monotonic_epoch;
};

/**
 * ElapsedTime - used to calculate realtime stats for use in performance monitoring. Will _not_ be effected by simulated time.
 * 
 */
struct ElapsedTime {

};
#endif
}