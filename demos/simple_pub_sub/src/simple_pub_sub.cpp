

#include "generated/include/simple_pub.h"
#include "generated/include/simple_sub.h"

#include <basis/core/coordinator.h>

#include <spdlog/spdlog.h>

//----------------- simple_pub -----------------

unit::simple_pub::SimplePub::Output
simple_pub::SimplePub(const unit::simple_pub::SimplePub::Input &input) {
  unit::simple_pub::SimplePub::Output output;
  return output;
}

void pub() {
  spdlog::info("Starting the publisher");

  simple_pub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  unit::simple_pub::SimplePub::Output output;
  auto msg = std::make_shared<StringMessage>();
  msg->set_message("hello");
  output.chatter = msg;

  while (true) {
    unit.SimplePub_pubsub.OnOutput(output);
    unit.Update(1);
  }
}

//----------------- simple_sub -----------------

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

//----------------- coordinator -----------------

void coord() {
  spdlog::info("Starting the coordinator");

  auto coordinator = basis::core::transport::Coordinator::Create();
  if (!coordinator) {
    spdlog::error("Failed to create the coordinator");
    return;
  }

  while (true) {
    coordinator->Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

//----------------- main -----------------
int main(int argc, char *argv[]) {

  if (argc < 2) {
    spdlog::error("Usage: simple_pub_sub --pub|--sub");
    return 1;
  }

  if (strcmp(argv[1], "pub") == 0) {
    pub();
  } else if (strcmp(argv[1], "sub") == 0) {
    sub();
  } else if (strcmp(argv[1], "coord") == 0) {
    coord();
  } else {
    spdlog::error("Usage: simple_pub_sub pub|sub|coord");
    return 1;
  }

  return 0;
}