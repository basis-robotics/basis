#pragma once

#include <cstdint>
#include <list>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <transport.pb.h>
#pragma clang diagnostic pop

#include <basis/plugins/serialization/protobuf.h>
#include <basis/plugins/transport/tcp.h>

#include "coordinator_default_port.h"

#include <iostream>

namespace basis::core::transport {

/**
 * Class to maintain a connection to a coordinator.
 */
class CoordinatorConnector : basis::plugins::transport::TcpSender {
  using TcpSender::TcpSender;

public:
  static std::unique_ptr<CoordinatorConnector> Create(uint16_t port = BASIS_PUBLISH_INFO_PORT) {
    auto maybe_socket = networking::TcpSocket::Connect("127.0.0.1", port);
    if (maybe_socket) {
      return std::make_unique<CoordinatorConnector>(std::move(*maybe_socket));
    }
    return {};
  }

  void SendToCoordinator(const proto::ClientToCoordinatorMessage &message) {
    using Serializer = SerializationHandler<proto::ClientToCoordinatorMessage>::type;
    const size_t size = Serializer::GetSerializedSize(message);

    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, size);
    Serializer::SerializeToSpan(message, shared_message->GetMutablePayload());

    SendMessage(shared_message);
  }

  void SendTransportManagerInfo(const proto::TransportManagerInfo &info) {
    proto::ClientToCoordinatorMessage message;
    *message.mutable_transport_manager_info() = info;
    SendToCoordinator(message);
  }

  void SendSchemas(const std::vector<basis::core::serialization::MessageSchema> &schemas) {
    proto::ClientToCoordinatorMessage message;
    auto *schemas_msg = message.mutable_schemas();

    auto *schemas_buffer = schemas_msg->mutable_schemas();
    for (auto &schema : schemas) {
      auto *schema_msg = schemas_buffer->Add();
      schema_msg->set_serializer(schema.serializer);
      schema_msg->set_name(schema.name);
      schema_msg->set_schema(schema.schema);
      schema_msg->set_hash_id(schema.hash_id);
    }

    SendToCoordinator(message);
  }

  void RequestSchemas(std::span<const std::string> schema_ids) {
    // TODO: ideally this should work in the form of an RPC Promise
    proto::ClientToCoordinatorMessage message;
    auto *schemas_field = message.mutable_request_schemas();

    *schemas_field->mutable_schema_ids() = {schema_ids.begin(), schema_ids.end()};

    SendToCoordinator(message);
  }

  void Update() {
    // todo: just put this on tcpconnection??

    bool finished = false;
    do {
      switch (ReceiveMessage(in_progress_packet)) {
      case plugins::transport::TcpConnection::ReceiveStatus::DONE: {
        auto complete = in_progress_packet.GetCompletedMessage();
        auto message = basis::DeserializeFromSpan<proto::CoordinatorMessage>(complete->GetPayload());

        switch (message->PossibleMessages_case()) {
        case proto::CoordinatorMessage::PossibleMessagesCase::kError: {
          spdlog::debug("Error: {}", message->error());
          errors_from_coordinator.push_back(std::move(message->error()));
          break;
        }
        case proto::CoordinatorMessage::PossibleMessagesCase::kNetworkInfo: {
          last_network_info = std::unique_ptr<proto::NetworkInfo>(message->release_network_info());
          break;
        }
        case proto::CoordinatorMessage::PossibleMessagesCase::kSchemas: {
          for (auto &schema : message->schemas().schemas()) {
            network_schemas.emplace(schema.serializer() + ":" + schema.name(), schema);
          }
          break;
        }
        default: {
          spdlog::error("Unknown message from coordinator! {}", message->DebugString());
        }
        }
        break;
      }
      case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
        finished = true;
        break;
      }
      case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
        spdlog::error("connection error after bytes {} - got error {} {}", in_progress_packet.GetCurrentProgress(),
                      errno, strerror(errno));
      }
      case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
        finished = true;

        break;
      }
      }
    } while (!finished);
  }

  proto::NetworkInfo *GetLastNetworkInfo() { return last_network_info.get(); }

  /**
   * Mostly intended for utility use, for now. There's no infrastructure around knowing when your requested schema has
   * arrived.
   */
  proto::MessageSchema *TryGetSchema(std::string_view schema_id) {
    auto it = network_schemas.find(std::string(schema_id));
    if (it == network_schemas.end()) {
      return nullptr;
    }
    return &it->second;
  }

  std::vector<std::string> errors_from_coordinator;

protected:
  std::unique_ptr<proto::NetworkInfo> last_network_info;

  std::unordered_map<std::string, proto::MessageSchema> network_schemas;

  IncompleteMessagePacket in_progress_packet;
};
} // namespace basis::core::transport