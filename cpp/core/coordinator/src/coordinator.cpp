#include <numeric>

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
      auto msg = basis::DeserializeFromSpan<proto::ClientToCoordinatorMessage>(complete->GetPayload());
      if (!msg) {
        spdlog::error("Coordinator: failed to deserialize a message");
      } else {
        // todo: break these out into handlers for better unit testing
        if (msg->has_transport_manager_info()) {
          client.info = std::unique_ptr<proto::TransportManagerInfo>(msg->release_transport_manager_info());
        } else if (msg->has_schemas()) {
          for (auto &schema : msg->schemas().schemas()) {
            std::string key = schema.serializer() + ":" + schema.name();
            if (!known_schemas.contains(key)) {
              known_schemas.emplace(std::move(key), std::move(schema));
            }
          }
        } else if (msg->has_request_schemas()) {
          std::vector<std::string> errors;
          std::vector<proto::MessageSchema> schemas;
          for (const auto &request : msg->request_schemas().schema_ids()) {
            auto it = known_schemas.find(request);
            if (it == known_schemas.end()) {
              errors.push_back(request);
            } else {
              schemas.push_back(it->second);
            }
          }
          
          if (errors.size()) {
            auto comma_fold = [](std::string a, std::string b) { return std::move(a) + ", " + b; };

            std::string s = std::accumulate(std::next(errors.begin()), errors.end(),
                                            "?!Missing schemas: " + errors[0], // start with first element
                                            comma_fold);
            proto::CoordinatorMessage errors_message;

            errors_message.set_error(s);
            spdlog::error("{} !", errors_message.DebugString());
            auto shared_message = SerializeMessagePacket(errors_message);
            spdlog::error("error size {}", shared_message->GetPacket().size());
            client.SendMessage(shared_message);
          }
          if (schemas.size()) {
            proto::CoordinatorMessage schema_message;
            *schema_message.mutable_schemas()->mutable_schemas() = {schemas.begin(), schemas.end()};
            spdlog::error("Got it!{}", schema_message.DebugString());

            client.SendMessage(SerializeMessagePacket(schema_message));
          }
        } else {
          spdlog::error("Unknown message from client!");
        }
      }
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

  proto::CoordinatorMessage message;
  // Compile the information
  *message.mutable_network_info() = GenerateNetworkInfo();

  // Serialize it
  auto shared_message = SerializeMessagePacket(message);

  // Send to all contacts
  for (auto &client : clients) {
    client.SendMessage(shared_message);
  }
}

} // namespace basis::core::transport