
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <simple_pub_sub.pb.h>
#pragma clang diagnostic pop

#include <basis/core/transport/transport_manager.h>
#include <basis/plugins/transport/tcp.h>

#include "../generated/unit/simple_pub/handlers/SimplePub.h"

using namespace unit::simple_pub::SimplePub;

int main() {
  PubSub pubsub(nullptr);

  basis::core::transport::TransportManager transport_manager(
      std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport(
      "net_tcp", std::make_unique<basis::plugins::transport::TcpTransport>());

  pubsub.SetupPubSub(&transport_manager, nullptr, nullptr);

  auto msg = std::make_shared<StringMessage>();
  msg->set_message("Hello, world!");
  pubsub.out_message_publisher->Publish(msg);
  return 0;
}