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
  BASIS_LOG_INFO("Got timetest");
  out.time_test_out = input.time_test_time;
  return out;
}

ApproxTest::Output test_unit::ApproxTest(const ApproxTest::Input &input) {
  BASIS_LOG_INFO("Got approximate output");
  return {};
}

TestEqualOptions::Output
test_unit::TestEqualOptions(const TestEqualOptions::Input &input) {
  BASIS_LOG_INFO("Got approximate output");
  return {};
}


TestInprocTypePub::Output test_unit::TestInprocTypePub(const TestInprocTypePub::Input& input) {
  BASIS_LOG_INFO("TestInprocTypePub");
  return {input.inproc_test_trigger};
}

TestInprocTypeSubEither::Output test_unit::TestInprocTypeSubEither(const TestInprocTypeSubEither::Input& input) {
  test_inproc_either_variant_executed = true;
  test_inproc_variant_index = input.inproc_test.index();
  BASIS_LOG_INFO("TestInprocTypeSubEither");
  return {};
}

TestInprocTypeSubOnlyMessage::Output test_unit::TestInprocTypeSubOnlyMessage(const TestInprocTypeSubOnlyMessage::Input& input) {
  test_inproc_only_message_executed = true;
  BASIS_LOG_INFO("TestInprocTypeSubOnlyMessage");
  return {};
}

TestInprocTypeSubOnlyInproc::Output test_unit::TestInprocTypeSubOnlyInproc(const TestInprocTypeSubOnlyInproc::Input& input) {
  test_inproc_only_inproc_executed = true;
  BASIS_LOG_INFO("TestInprocTypeSubOnlyInproc");
  return {};
}

TestInprocTypeSubAccumulate::Output test_unit::TestInprocTypeSubAccumulate(const TestInprocTypeSubAccumulate::Input& input) {
  test_inproc_accumulated_input_executed = true;
  test_inproc_accumulated_input = std::move(input.inproc_test);
  BASIS_LOG_INFO("TestInprocTypeSubAccumulate");
  return {};
}
TestInprocTypeSubAvoidPointlessConversion::Output test_unit::TestInprocTypeSubAvoidPointlessConversion(const TestInprocTypeSubAvoidPointlessConversion::Input& input) {
  test_inproc_avoid_pointless_conversion_executed = true;
  BASIS_LOG_INFO("TestInprocTypeSubAvoidPointlessConversion");
  return {};
}
