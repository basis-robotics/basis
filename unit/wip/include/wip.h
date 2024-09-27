/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/wip/unit_base.h>

class wip : public unit::wip::Base {
public:
  wip(const unit::wip::Args &args,
      const std::optional<std::string_view> &unit_name_override = {})
      : unit::wip::Base(args, unit_name_override) {}

  virtual unit::wip::StereoMatch::Output
  StereoMatch(const unit::wip::StereoMatch::Input &input) override;

  virtual unit::wip::TimeTest::Output
  TimeTest(const unit::wip::TimeTest::Input &input) override;

  virtual unit::wip::ApproxTest::Output
  ApproxTest(const unit::wip::ApproxTest::Input &input) override;
};