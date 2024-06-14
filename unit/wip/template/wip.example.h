/*

  DO NOT EDIT THIS FILE

  This is a template for use with your Unit, to use as a base, provided as an example.

*/
#include <unit/wip/unit_base.h>

class wip : public unit::wip::Base {
  wip() {

  }


    virtual unit::wip::StereoMatch::Output StereoMatch(const unit::wip::StereoMatch::Input& input) override;

    virtual unit::wip::TimeTest::Output TimeTest(const unit::wip::TimeTest::Input& input) override;

    virtual unit::wip::ApproxTest::Output ApproxTest(const unit::wip::ApproxTest::Input& input) override;

};