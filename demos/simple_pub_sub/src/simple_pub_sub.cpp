
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <simple_pub_sub.pb.h>
#pragma clang diagnostic pop

#include <basis/core/transport/transport_manager.h>
#include <basis/plugins/transport/tcp.h>

#include "generated/unit/simple_pub/handlers/SimplePub.h"
#include "generated/unit/simple_sub/handlers/SimpleSub.h"

void setupPub(basis::core::transport::TransportManager &transport_manager) {
  using namespace unit::simple_pub::SimplePub;
  PubSub pubsub(nullptr);

  pubsub.SetupPubSub(&transport_manager, nullptr, nullptr);

  auto msg = std::make_shared<StringMessage>();
  msg->set_message("Hello, world!");
  pubsub.out_message_publisher->Publish(msg);
}

void setupSub(basis::core::transport::TransportManager &transport_manager) {
  using namespace unit::simple_sub::SimpleSub;
  PubSub pubsub(nullptr);

  pubsub.SetupPubSub(&transport_manager, nullptr, nullptr);


}

int main() {
  basis::core::transport::TransportManager transport_manager(
      std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport(
      "net_tcp", std::make_unique<basis::plugins::transport::TcpTransport>());

  setupPub(transport_manager);
  return 0;
}