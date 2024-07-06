/*

  This is the starting point for your Unit. Edit this directly and implement the missing methods!

*/
#include <unit/simple_sub/unit_base.h>

class simple_sub : public unit::simple_sub::Base {
public:
  simple_sub() : unit::simple_sub::Base() {};


    virtual unit::simple_sub::OnChatter::Output OnChatter(const unit::simple_sub::OnChatter::Input& input) override;

};