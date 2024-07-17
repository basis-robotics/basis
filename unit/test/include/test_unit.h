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
};