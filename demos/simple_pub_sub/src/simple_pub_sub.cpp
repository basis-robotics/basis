

#include "generated/include/simple_pub.h"
#include "generated/include/simple_sub.h"

#include <basis/core/coordinator.h>

#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

unit::simple_pub::SimplePub::Output
simple_pub::SimplePub(const unit::simple_pub::SimplePub::Input &input) {
  unit::simple_pub::SimplePub::Output output;
  return output;
}

void pub() {
  spdlog::info("Starting the publisher");

  basis::core::transport::Coordinator coordinator =
      *basis::core::transport::Coordinator::Create();

  simple_pub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  coordinator.Update();

  unit::simple_pub::SimplePub::Output output;
  std::shared_ptr<StringMessage> msg = std::make_shared<StringMessage>();
  msg->set_message("hello");
  output.chatter = msg;
  unit.SimplePub_pubsub.OnOutput(output);

  while (true) {
    coordinator.Update();
    unit.SimplePub_pubsub.OnOutput(output);
    unit.Update(1);
  }
}

unit::simple_sub::SimpleSub::Output
simple_sub::SimpleSub(const unit::simple_sub::SimpleSub::Input &input) {
  spdlog::info("Got a message: {}", input.chatter->message());
  unit::simple_sub::SimpleSub::Output output;
  return output;
}

void sub() {
  spdlog::info("Starting the subscriber");

  simple_sub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  while (true) {
    unit.Update(1);
  }
}

int main(int argc, char *argv[]) {

  argparse::ArgumentParser parser("simple_pub_sub");
  parser.add_argument("--pub").default_value(false).implicit_value(true);

  parser.parse_args(argc, argv);

  if (parser["--pub"] == true) {
    pub();
  } else {
    sub();
  }

  return 0;
}