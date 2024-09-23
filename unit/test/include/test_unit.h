/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/test_unit/unit_base.h>

class test_unit : public unit::test_unit::Base {
public:
  test_unit(std::optional<std::string> name_override = {})
      : unit::test_unit::Base(name_override) {}

  virtual unit::test_unit::StereoMatch::Output
  StereoMatch(const unit::test_unit::StereoMatch::Input &input) override;

  virtual unit::test_unit::AllTest::Output
  AllTest(const unit::test_unit::AllTest::Input &input) override;

  virtual unit::test_unit::ApproxTest::Output
  ApproxTest(const unit::test_unit::ApproxTest::Input &input) override;

  virtual unit::test_unit::TestEqualOptions::Output TestEqualOptions(
      const unit::test_unit::TestEqualOptions::Input &input) override;

  virtual unit::test_unit::TestInprocTypePub::Output
  TestInprocTypePub(const unit::test_unit::TestInprocTypePub::Input &input) override;

  bool test_inproc_either_variant_executed = false;
  int test_inproc_variant_index = -1;
  virtual unit::test_unit::TestInprocTypeSubEither::Output
  TestInprocTypeSubEither(const unit::test_unit::TestInprocTypeSubEither::Input &input) override;

  bool test_inproc_only_inproc_executed = false;
  virtual unit::test_unit::TestInprocTypeSubOnlyInproc::Output
  TestInprocTypeSubOnlyInproc(const unit::test_unit::TestInprocTypeSubOnlyInproc::Input &input) override;

  bool test_inproc_only_message_executed = false;
  virtual unit::test_unit::TestInprocTypeSubOnlyMessage::Output
  TestInprocTypeSubOnlyMessage(const unit::test_unit::TestInprocTypeSubOnlyMessage::Input &input) override;

  bool test_inproc_accumulated_input_executed = false;
  decltype(unit::test_unit::TestInprocTypeSubAccumulate::Input::inproc_test) test_inproc_accumulated_input;
  virtual unit::test_unit::TestInprocTypeSubAccumulate::Output
  TestInprocTypeSubAccumulate(const unit::test_unit::TestInprocTypeSubAccumulate::Input &input) override;

  bool test_inproc_avoid_pointless_conversion_executed = false;
  virtual unit::test_unit::TestInprocTypeSubAvoidPointlessConversion::Output
  TestInprocTypeSubAvoidPointlessConversion(const unit::test_unit::TestInprocTypeSubAvoidPointlessConversion::Input &input) override;
};