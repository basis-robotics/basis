/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/test_unit/unit_base.h>

class test_unit : public unit::test_unit::Base {
public:
  test_unit() {}

  virtual unit::test_unit::StereoMatch::Output
  StereoMatch(const unit::test_unit::StereoMatch::Input &input) override;

  virtual unit::test_unit::TimeTest::Output
  TimeTest(const unit::test_unit::TimeTest::Input &input) override;

  virtual unit::test_unit::ApproxTest::Output
  ApproxTest(const unit::test_unit::ApproxTest::Input &input) override;
};