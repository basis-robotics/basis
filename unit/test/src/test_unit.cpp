/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/

#include <test_unit.h>

using namespace unit::test_unit;

StereoMatch::Output test_unit::StereoMatch(const StereoMatch::Input &input) {
  StereoMatch::Output out;
  return out;
}

unit::test_unit::TimeTest::Output test_unit::TimeTest(const unit::test_unit::TimeTest::Input &input) {
  unit::test_unit::TimeTest::Output out;
  spdlog::info("Got timetest");
  out.time_test_forwarded_2 = input.time_test_forwarded;
  return out;
}

ApproxTest::Output test_unit::ApproxTest(const ApproxTest::Input& input) {
  spdlog::info("Got approximate output");
  return {};
}
