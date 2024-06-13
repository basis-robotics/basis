/*

  This is the starting point for your Unit. Edit this directly and implement the missing methods!

*/
#include <unit/wip/unit_base.h>

struct wip : public unit::wip::Base {
  wip() {

  }


    virtual unit::wip::StereoMatch::Output StereoMatch(const unit::wip::StereoMatch::Input& input) override;

};