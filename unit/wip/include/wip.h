/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/wip/unit_base.h>

class wip : public unit::wip::Base {
public:
  wip() {}

  virtual unit::wip::StereoMatch::Output
  StereoMatch(const unit::wip::StereoMatch::Input &input) override;

  virtual unit::wip::TimeTest::Output
  TimeTest(const unit::wip::TimeTest::Input &input) override;

  virtual unit::wip::ApproxTest::Output
  ApproxTest(const unit::wip::ApproxTest::Input &input) override;
};

// TODO: move to template??
extern "C" {

basis::Unit* CreateUnit() {
    return new wip();
}

}