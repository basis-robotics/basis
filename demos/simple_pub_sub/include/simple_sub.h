/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/simple_sub/unit_base.h>

class simple_sub : public unit::simple_sub::Base {
public:
  simple_sub(const unit::simple_sub::Args& args, const std::optional<std::string_view>& name_override = {})
      : unit::simple_sub::Base(args, name_override) {}

  virtual unit::simple_sub::OnChatter::Output
  OnChatter(const unit::simple_sub::OnChatter::Input &input) override;
};