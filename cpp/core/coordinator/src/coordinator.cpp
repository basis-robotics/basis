#include <numeric>

#include <basis/core/coordinator.h>

DECLARE_AUTO_LOGGER_NS(basis::core::transport::coordinator)

namespace basis::core::transport {

std::optional<Coordinator> Coordinator::Create(uint16_t port) {
  // todo: maybe return the listen socket error type?
  auto maybe_listen_socket = networking::TcpListenSocket::Create(port);
  if (!maybe_listen_socket) {
    BASIS_LOG_ERROR_NS(coordinator, "Coordinator: Unable to create listen socket on port {}", port);
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
  // TODO: refactor this to select on all sockets - allows passing in a sleep time to Update()

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
        BASIS_LOG_ERROR_NS(coordinator, "Coordinator: failed to deserialize a message");
      } else {
        // todo: break these out into handlers for better unit testing
        switch (msg->PossibleMessages_case()) {
        case proto::ClientToCoordinatorMessage::kTransportManagerInfo:
          HandleTransportManagerInfoRequest(msg->release_transport_manager_info(), client);
          break;

        case proto::ClientToCoordinatorMessage::kSchemas:
          HandleSchemasRequest(msg->schemas());
          break;

        case proto::ClientToCoordinatorMessage::kRequestSchemas:
          HandleRequestSchemasRequest(msg->request_schemas(), client);
          break;

        case proto::ClientToCoordinatorMessage::POSSIBLEMESSAGES_NOT_SET:
          BASIS_LOG_ERROR_NS(coordinator, "Unknown message from client!");
          break;
        }
      }
      // BASIS_LOG_DEBUG_NS(coordinator, "Got completed message {}", client.info->DebugString());
      [[fallthrough]];
    }
    case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
      // No work to be done
      ++it;
      break;
    }
    case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
      BASIS_LOG_ERROR_NS(coordinator, "Client connection error after bytes {} - got error {} {}",
                    client.in_progress_packet.GetCurrentProgress(), errno, strerror(errno));
      // TODO: we can do a fast delete here instead, swapping with .last()
      it = clients.erase(it);
      break;
    }
    case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
      BASIS_LOG_ERROR_NS(coordinator, "Client connection disconnect after {} bytes", client.in_progress_packet.GetCurrentProgress());
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

void Coordinator::HandleTransportManagerInfoRequest(proto::TransportManagerInfo *transport_manager_info,
                                                    Connection &client) {
  client.info = std::unique_ptr<proto::TransportManagerInfo>(transport_manager_info);
}

void Coordinator::HandleSchemasRequest(const proto::MessageSchemas &schemas) {
  for (auto &schema : schemas.schemas()) {
    std::string key = schema.serializer() + ":" + schema.name();
    if (!known_schemas.contains(key)) {
      BASIS_LOG_INFO_NS(coordinator, "Adding schema {}", key);
      known_schemas.emplace(std::move(key), std::move(schema));
    }
  }
}

void Coordinator::HandleRequestSchemasRequest(const proto::RequestSchemas &request_schemas, Connection &client) {

  std::vector<std::string> errors;
  std::vector<proto::MessageSchema> schemas;
  for (const auto &request : request_schemas.schema_ids()) {
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
                                    "Missing schemas: " + errors[0], // start with first element
                                    comma_fold);
    proto::CoordinatorMessage errors_message;

    errors_message.set_error(s);
    auto shared_message = SerializeMessagePacket(errors_message);
    client.SendMessage(shared_message);
  }
  if (schemas.size()) {
    proto::CoordinatorMessage schema_message;
    *schema_message.mutable_schemas()->mutable_schemas() = {schemas.begin(), schemas.end()};

    client.SendMessage(SerializeMessagePacket(schema_message));
  }
}

} // namespace basis::core::transport