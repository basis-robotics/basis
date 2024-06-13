/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/

#include <wip.h>

using namespace unit::wip;

StereoMatch::Output wip::StereoMatch(const StereoMatch::Input &input) {
  StereoMatch::Output out;
  return out;
}

unit::wip::TimeTest::Output
wip::TimeTest(const unit::wip::TimeTest::Input &input) {
  unit::wip::TimeTest::Output out;
  spdlog::info("Got timetest");
  out.time_test_forwarded_2 = input.time_test_forwarded;
  return out;
}
