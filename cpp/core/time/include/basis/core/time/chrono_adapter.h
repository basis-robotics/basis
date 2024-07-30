#pragma once
#include <basis/core/time.h>
#include <chrono>

namespace basis {
namespace time {
struct ChronoClock {
  typedef std::chrono::nanoseconds duration;
  typedef duration::rep rep;
  typedef duration::period period;
  typedef std::chrono::time_point<ChronoClock> time_point;
  static const bool is_steady = true;

  static time_point now() noexcept {
    using namespace std::chrono;
    return time_point(std::chrono::nanoseconds(basis::core::MonotonicTime::Now().nsecs));
  }

  static std::time_t to_time_t(const time_point &t) noexcept {
    return std::chrono::time_point_cast<std::chrono::seconds>(t).time_since_epoch().count();
  }
};
} // namespace time
} // namespace basis