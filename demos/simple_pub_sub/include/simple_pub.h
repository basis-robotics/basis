/*

  This is the starting point for your Unit. Edit this directly and implement the missing methods!

*/
#include <unit/simple_pub/unit_base.h>

class simple_pub : public unit::simple_pub::Base {
public:
  simple_pub() {

  }


    virtual unit::simple_pub::SimplePub::Output SimplePub(const unit::simple_pub::SimplePub::Input& input) override;

};