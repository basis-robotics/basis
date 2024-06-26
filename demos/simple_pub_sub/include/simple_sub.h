/*

  This is the starting point for your Unit. Edit this directly and implement the missing methods!

*/
#include <unit/simple_sub/unit_base.h>

class simple_sub : public unit::simple_sub::Base {
public:
  simple_sub() {

  }


    virtual unit::simple_sub::SimpleSub::Output SimpleSub(const unit::simple_sub::SimpleSub::Input& input) override;

};