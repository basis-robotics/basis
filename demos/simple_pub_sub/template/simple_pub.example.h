/*

  DO NOT EDIT THIS FILE

  This is a template for use with your Unit, to use as a base, provided as an example.

*/
#include <unit/simple_pub/unit_base.h>

class simple_pub : public unit::simple_pub::Base {
public:
  simple_pub() {

  }


    virtual unit::simple_pub::SimplePub::Output SimplePub(const unit::simple_pub::SimplePub::Input& input) override;

};