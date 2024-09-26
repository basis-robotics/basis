/*

  DO NOT EDIT THIS FILE

  This is a template for use with your Unit, to use as a base, provided as an example.

*/
#include <unit/simple_sub/unit_base.h>

class simple_sub : public unit::simple_sub::Base {
public:
  simple_sub(const std::optional<std::string_view>& name_override = {}) 
  : unit::simple_sub::Base(name_override)
  {}


  virtual unit::simple_sub::OnChatter::Output
  OnChatter(const unit::simple_sub::OnChatter::Input &input) override;

};