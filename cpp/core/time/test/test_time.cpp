#include <gtest/gtest.h>

#include <basis/core/time.h>

namespace basis::core {

struct TestTime : public MonotonicTime {};

TEST(TestTime, TestConversions) {
  RealTimeDuration invalid;
  ASSERT_FALSE(invalid.IsValid());

  RealTimeDuration one_sec = RealTimeDuration::FromSeconds(1.0);
  ASSERT_TRUE(one_sec.IsValid());
  ASSERT_EQ(one_sec.nsecs, 1000000000);
  ASSERT_EQ(one_sec.ToSeconds(), 1.0);

  RealTimeDuration minus_one_sec = RealTimeDuration::FromSeconds(-1.0);
  ASSERT_TRUE(minus_one_sec.IsValid());
  ASSERT_EQ(minus_one_sec.nsecs, -1000000000);
  ASSERT_EQ(minus_one_sec.ToSeconds(), -1.0);

  RealTimeDuration minus_10_25 = RealTimeDuration::FromSeconds(-10.25);
  ASSERT_TRUE(minus_10_25.IsValid());
  ASSERT_EQ(minus_10_25.nsecs, -10250000000);
  ASSERT_EQ(minus_10_25.ToSeconds(), -10.25);

  // Test a bunch of numbers here
  for (int i = -10000000; i < 10000001; i++) {
    RealTimeDuration d = RealTimeDuration::FromSeconds(i);
    ASSERT_TRUE(d.IsValid()) << i;
    // Within this range, we should still get perfect conversions on integers
    ASSERT_EQ(d.ToSeconds(), i) << i;
    if (i != 0) {
      RealTimeDuration d_frac = RealTimeDuration::FromSeconds(i + (double)1 / i);
      ASSERT_TRUE(d_frac.IsValid()) << i;
      // No promises on precision here
      ASSERT_FLOAT_EQ(d_frac.ToSeconds(), i + (double)1 / i) << i;
    }
  }

  for (int i = -1; i <= 1; i++) {

    TestTime right_now;
    // clang-format off
        int64_t now_total_nsecs = i * 1715386762'545773252;
        double now_double       = i * 1715386762.545773252;
        int64_t now_secs        = i * 1715386762;
        int64_t now_nsecs                  = i * 545773252;
    // clang-format on

    right_now.nsecs = now_total_nsecs;
    ASSERT_FLOAT_EQ(right_now.ToSeconds(), now_double);
    std::pair<uint32_t, uint32_t> now_ros(now_secs, now_nsecs);
    ASSERT_EQ(right_now.ToRosTime(), now_ros);

    timeval now_tv = {.tv_sec = now_secs, .tv_usec = now_nsecs * (std::nano::den / std::micro::den)};

    timeval converted_timeval = right_now.ToTimeval();
    ASSERT_EQ(converted_timeval.tv_sec, now_tv.tv_sec);
    ASSERT_EQ(converted_timeval.tv_usec, now_tv.tv_usec);
  }
}

} // namespace basis::core