/*

  This is the starting point for your Unit. Edit this directly and implement the
  missing methods!

*/
#include <unit/simple_pub/unit_base.h>

class simple_pub : public unit::simple_pub::Base {
public:
  simple_pub(const unit::simple_pub::Args& args, const std::optional<std::string_view>& name_override = {})
      : unit::simple_pub::Base(name_override) {}

  virtual unit::simple_pub::PublishAt1Hz::Output
  PublishAt1Hz(const unit::simple_pub::PublishAt1Hz::Input &input) override;
};