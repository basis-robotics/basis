/*

  This is the starting point for your Unit. Edit this directly and implement the missing methods!

*/
#include <unit/simple_sub/unit_base.h>

class simple_sub : public unit::simple_sub::Base {
public:
  simple_sub(std::optional<std::string> name_override = {}) 
  : unit::simple_sub::Base(name_override)
  {}
  

    virtual unit::simple_sub::OnChatter::Output OnChatter(const unit::simple_sub::OnChatter::Input& input) override;

};