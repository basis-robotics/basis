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

  void SendTransportManagerInfo(const proto::TransportManagerInfo &info) {
    proto::ClientToCoordinatorMessage message;
    *message.mutable_transport_manager_info() = info;
    using Serializer = SerializationHandler<proto::ClientToCoordinatorMessage>::type;
    const size_t size = Serializer::GetSerializedSize(message);

    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, size);
    Serializer::SerializeToSpan(message, shared_message->GetMutablePayload());

    SendMessage(shared_message);
  }

  void SendSchemas(const std::vector<basis::core::serialization::MessageSchema> &schemas) {
    proto::ClientToCoordinatorMessage message;
    auto *schemas_msg = message.mutable_schemas();

    auto& schemas_buffer = *schemas_msg->mutable_schemas();
    for(auto& schema : schemas) {
      auto* schema_msg = schemas_buffer.Add();
      schema_msg->set_serializer(schema.serializer);
      schema_msg->set_name(schema.name);
      schema_msg->set_schema(schema.schema);
      schema_msg->set_hash_id(schema.hash_id);
    }

    using Serializer = SerializationHandler<proto::ClientToCoordinatorMessage>::type;
    const size_t size = Serializer::GetSerializedSize(message);

    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, size);
    Serializer::SerializeToSpan(message, shared_message->GetMutablePayload());

    SendMessage(shared_message);
  }

  void Update() {
    // todo: just put this on tcpconnection??
    switch (ReceiveMessage(in_progress_packet)) {
    case plugins::transport::TcpConnection::ReceiveStatus::DONE: {
      auto complete = in_progress_packet.GetCompletedMessage();
      last_network_info = basis::DeserializeFromSpan<proto::NetworkInfo>(complete->GetPayload());
      spdlog::debug("Got network info message {}", last_network_info->DebugString());

      // convert to proto::PublisherInfo
      // fallthrough
    }
    case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
      break;
    }
    case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
      spdlog::error("connection error after bytes {} - got error {} {}", in_progress_packet.GetCurrentProgress(), errno,
                    strerror(errno));
      // fallthrough
    }
    case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
      // TODO: we can do a fast delete here instead, swapping with .last()
      break;
    }
    }
  }

  proto::NetworkInfo *GetLastNetworkInfo() { return last_network_info.get(); }

protected:
  std::unique_ptr<proto::NetworkInfo> last_network_info;

  IncompleteMessagePacket in_progress_packet;
};
} // namespace basis::core::transport