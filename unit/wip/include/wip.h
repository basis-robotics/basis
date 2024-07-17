/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/wip/unit_base.h>

class wip : public unit::wip::Base {
public:
  wip(std::optional<std::string> name_override = {})
      : unit::wip::Base(name_override) {}

  virtual unit::wip::StereoMatch::Output
  StereoMatch(const unit::wip::StereoMatch::Input &input) override;

  virtual unit::wip::TimeTest::Output
  TimeTest(const unit::wip::TimeTest::Input &input) override;

  virtual unit::wip::ApproxTest::Output
  ApproxTest(const unit::wip::ApproxTest::Input &input) override;
};