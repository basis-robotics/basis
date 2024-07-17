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

unit::test_unit::AllTest::Output
test_unit::AllTest(const unit::test_unit::AllTest::Input &input) {
  unit::test_unit::AllTest::Output out;
  spdlog::info("Got timetest");
  out.time_test_out = input.time_test_time;
  return out;
}

ApproxTest::Output test_unit::ApproxTest(const ApproxTest::Input &input) {
  spdlog::info("Got approximate output");
  return {};
}

TestEqualOptions::Output
test_unit::TestEqualOptions(const TestEqualOptions::Input &input) {
  spdlog::info("Got approximate output");
  return {};
}