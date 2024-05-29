#include <basis/core/coordinator.h>
namespace basis::core::transport {
std::optional<Coordinator> Coordinator::Create(uint16_t port) {
  // todo: maybe return the listen socket error type?
  auto maybe_listen_socket = networking::TcpListenSocket::Create(port);
  if (!maybe_listen_socket) {
    spdlog::error("Coordinator: Unable to create listen socket on port {}", port);
    return {};
  }

  return Coordinator(std::move(maybe_listen_socket.value()));
}
proto::NetworkInfo Coordinator::GenerateNetworkInfo() {
  proto::NetworkInfo out;

  for (const auto &client : clients) {
    if (client.info) {
      for (auto &publisher : client.info->publishers()) {
        *(*out.mutable_publishers_by_topic())[publisher.topic()].add_publishers() = publisher;
      }
    }
  }

  return out;
}

void Coordinator::Update() {
  // Look for new clients
  while (auto maybe_socket = listen_socket.Accept(0)) {
    clients.emplace_back(std::move(maybe_socket.value()));
  }

  // Receive messages from each client
  for (auto it = clients.begin(); it != clients.end();) {
    auto &client = *it;
    switch (client.ReceiveMessage(client.in_progress_packet)) {
    case plugins::transport::TcpConnection::ReceiveStatus::DONE: {
      auto complete = client.in_progress_packet.GetCompletedMessage();
      client.info = basis::DeserializeFromSpan<proto::TransportManagerInfo>(complete->GetPayload());
      spdlog::debug("Got completed message {}", client.info->DebugString());
      [[fallthrough]];
    }
    case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
      // No work to be done
      ++it;
      break;
    }
    case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
      spdlog::error("Client connection error after bytes {} - got error {} {}",
                    client.in_progress_packet.GetCurrentProgress(), errno, strerror(errno));
      // TODO: we can do a fast delete here instead, swapping with .last()
      it = clients.erase(it);
      break;
    }
    case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
      spdlog::error("Client connection disconnect after {} bytes", client.in_progress_packet.GetCurrentProgress());
      // TODO: we can do a fast delete here instead, swapping with .last()
      it = clients.erase(it);
      break;
    }
    }
  }

  // Compile the information
  proto::NetworkInfo info = GenerateNetworkInfo();

  // Serialize it
  using Serializer = SerializationHandler<proto::TransportManagerInfo>::type;
  const size_t size = Serializer::GetSerializedSize(info);

  auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, size);
  Serializer::SerializeToSpan(info, shared_message->GetMutablePayload());

  // Send to all contacts
  for (auto &client : clients) {
    client.SendMessage(shared_message);
  }
}

} // namespace basis::core::transport