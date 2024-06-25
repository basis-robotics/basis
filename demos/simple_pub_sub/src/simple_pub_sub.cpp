
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <simple_pub_sub.pb.h>
#pragma clang diagnostic pop

// #include <basis/core/transport/transport_manager.h>
// #include <basis/plugins/transport/tcp.h>

// #include "generated/unit/simple_pub/handlers/SimplePub.h"
// #include "generated/unit/simple_sub/handlers/SimpleSub.h"

#include "generated/include/simple_pub.h"
#include "generated/include/simple_sub.h"

unit::simple_pub::SimplePub::Output
simple_pub::SimplePub(const unit::simple_pub::SimplePub::Input &input) {
  unit::simple_pub::SimplePub::Output output;
  std::shared_ptr<StringMessage> msg = std::make_shared<StringMessage>();
  msg->set_message("hello");
  output.out_message = msg;
  return output;
}

void pub() {
  simple_pub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  while (true) {
    unit.Update(1);
  }
}

unit::simple_sub::SimpleSub::Output
simple_sub::SimpleSub(const unit::simple_sub::SimpleSub::Input &input) {
  unit::simple_sub::SimpleSub::Output output;
  return output;
}

void sub() {
  simple_sub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  while (true) {
    unit.Update(1);
  }
}

int main() {
  pub();
  sub();
  return 0;
}