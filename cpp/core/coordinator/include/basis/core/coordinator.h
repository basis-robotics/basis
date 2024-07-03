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

/**
 * This is a bit annoying. We have to create a custom transport for publisher info as both the coordinator socket needs
 * to be at a well known location, as well as accepting data bidirectionally, something that wasn't intended by the tcp
 * transport design.
 *
 * Maybe later once we have proper duplex on the tcp side we can just use a regular tcp receiver.
 *
 * In any case, this means that the coordinator will be a completely custom stack, for now. In theory one _can_ make a
 * coordinator using the pub/sub framework, but there are a lot of chicken and egg problems here.
 *
 * Things that will need solved:
 *   - per transport configuration (for the well known port)
 *   - duplexed communication between
 *      - how? because of epoll we probably don't want to have two objects sharing a socket.
 *          - Likely it would mean combining sender and receiver, and letting epoll have differing callbacks for getting
 * and sending data.
 *          - this work has been started
 *      - this has an additional complication - the send and recv side are different types
 *   - this is really just an RPC system
 *
 * The short term dream here is to only use special constructions for the publisher info and spin up proper infra for
 * every other bit of information. The publish info stuff should take minimal processing and run on a single thread. It
 * will represent a small snag on the unit side - will need a helper class to assist with coordinator communication.
 *
 * TODO: it might be simpler to just write this as a protobuf grpc server :)
 */

namespace basis::core::transport {
/**
 * A utility class for communicating the topic network state between TransportManagers (typically one per process)
 *
 * Implemented single threaded for safety - latency isn't a huge concern here.
 *
 * @todo track publishers and add a delta message
 * @todo on connection, send the last message again (can we just implement latching?)
 */
class Coordinator {
  /**
   * An internal connection to a TransportManager
   */
  struct Connection : public basis::plugins::transport::TcpSender {
    Connection(core::networking::TcpSocket &&socket) : TcpSender(std::move(socket)) { socket.SetNonblocking(); }

    IncompleteMessagePacket in_progress_packet;

    std::unique_ptr<proto::TransportManagerInfo> info;

    // todo: add rate limiting here to sending the publishers
  };

public:
  /**
   * Creates a coordinator.
   *
   * @param port overridable - by default BASIS_PUBLISH_INFO_PORT
   * @return a Coordinator, or std::nullopt if creation fails (usually due to the port already being taken)
   */
  static std::optional<Coordinator> Create(uint16_t port = BASIS_PUBLISH_INFO_PORT);

  Coordinator(networking::TcpListenSocket &&listen_socket) : listen_socket(std::move(listen_socket)) {}

  /**
   * Gathers the publisher information from each client and packages it up into one message.
   */
  proto::NetworkInfo GenerateNetworkInfo();

  /**
   * Update the coordinator. Should be called on a regular basis, from the main thread.
   *
   * Accept new clients
   * Compiles data about the subscription networks
   * Sends the data to each client
   */
  void Update();

  const std::unordered_map<std::string, proto::MessageSchema> &GetKnownSchemas() const { return known_schemas; }

protected:
  std::shared_ptr<basis::core::transport::MessagePacket>
  SerializeMessagePacket(const proto::CoordinatorMessage &message) {
    using Serializer = SerializationHandler<proto::CoordinatorMessage>::type;
    const size_t size = Serializer::GetSerializedSize(message);

    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, size);
    Serializer::SerializeToSpan(message, shared_message->GetMutablePayload());
    return shared_message;
  }

  void HandleTransportManagerInfoRequest(proto::TransportManagerInfo *transport_manager_info, Connection &client);
  void HandleSchemasRequest(const proto::MessageSchemas &schemas);
  void HandleRequestSchemasRequest(const proto::RequestSchemas &request_schemas, Connection &client);

  /**
   * The TCP listen socket.
   */
  core::networking::TcpListenSocket listen_socket;

  /**
   * The clients we should send information to. On disconnection, will be removed by Update()
   */
  std::list<Connection> clients;

  /**
   * All known schemas, indexed by "encoder_name:schema_name"
   */
  std::unordered_map<std::string, proto::MessageSchema> known_schemas;
};

} // namespace basis::core::transport