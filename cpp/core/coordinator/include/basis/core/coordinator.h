#pragma once

#include <cstdint>
#include <list>

#include <basis/plugins/serialization/protobuf.h>
#include <basis/plugins/transport/tcp.h>

#include <transport.pb.h>

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
constexpr uint16_t BASIS_PUBLISH_INFO_PORT = 1492;

// constexpr char BASIS_PUBLISHER_INFO_TOPIC[] = "/basis/publisher_info";

namespace basis::core::transport {
/**
 *
 * Implemented single threaded for safety - latency isn't a huge concern here.
 *
 * @todo track publishers and add a delta message
 */
class Coordinator {
  struct Connection : public basis::plugins::transport::TcpSender {
    Connection(core::networking::TcpSocket socket) : TcpSender(std::move(socket)) { socket.SetNonblocking(); }

    IncompleteMessagePacket in_progress_packet;

    std::unique_ptr<proto::TransportManagerInfo> info;

    // todo: add rate limiting here to sending the publishers
  };

public:
  static std::optional<Coordinator> Create(uint16_t port = BASIS_PUBLISH_INFO_PORT) {
    // todo: maybe return the listen socket error type?
    auto maybe_listen_socket = networking::TcpListenSocket::Create(port);
    if (!maybe_listen_socket) {
        spdlog::error("Coordinator: Unable to create listen socket on port {}");
      return {};
    }

    return Coordinator(std::move(maybe_listen_socket.value()));
  }

  Coordinator(networking::TcpListenSocket listen_socket) : listen_socket(std::move(listen_socket)) {}

  proto::NetworkInfo GenerateNetworkInfo() {
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

  void Update() {
    // accept new clients
    // select on all clients
    // compile new data on publishers

    //
    while (auto maybe_socket = listen_socket.Accept(0)) {
      clients.emplace_back(std::move(maybe_socket.value()));
    }

    for (auto it = clients.begin(); it != clients.end();) {
      auto &client = *it;
      switch (client.ReceiveMessage(client.in_progress_packet)) {
      case plugins::transport::TcpConnection::ReceiveStatus::DONE: {
        auto complete = client.in_progress_packet.GetCompletedMessage();
        client.info = basis::DeserializeFromSpan<proto::TransportManagerInfo>(complete->GetPayload());
        spdlog::info("Got completed message {}", client.info->DebugString());

        // convert to proto::PublisherInfo
        // fallthrough
      }
      case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
        // No work to be done
        ++it;
        break;
      }
      case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
        spdlog::error("Client connection error after bytes {} - got error {} {}",
                      client.in_progress_packet.GetCurrentProgress(), errno, strerror(errno));
        it = clients.erase(it);
        break;
              }
      case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
                spdlog::error("Client connection disconnect after {} bytes",
                      client.in_progress_packet.GetCurrentProgress());
        // TODO: we can do a fast delete here instead, swapping with .last()
        it = clients.erase(it);
        break;
      }
      }
    }

    proto::NetworkInfo info = GenerateNetworkInfo();

    using Serializer = SerializationHandler<proto::TransportManagerInfo>::type;
    const size_t size = Serializer::GetSerializedSize(info);

    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, size);
    Serializer::SerializeToSpan(info, shared_message->GetMutablePayload());

    for (auto &client : clients) {
      client.SendMessage(shared_message);
    }
  }

  core::networking::TcpListenSocket listen_socket;

  std::list<Connection> clients;
};

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
  // TODO: this logic is somewhat duplicated with TcpSender - need to refactor TcpSender to be able to track progress of
  // the current send
  void SendTransportManagerInfo(const proto::TransportManagerInfo &info) {
    using Serializer = SerializationHandler<proto::TransportManagerInfo>::type;
    const size_t size = Serializer::GetSerializedSize(info);

    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, size);
    Serializer::SerializeToSpan(info, shared_message->GetMutablePayload());

    SendMessage(shared_message);
  }

  void Update() {
    // todo: just put this on tcpconnection??
    switch (ReceiveMessage(in_progress_packet)) {
      case plugins::transport::TcpConnection::ReceiveStatus::DONE: {
        auto complete = in_progress_packet.GetCompletedMessage();
        last_network_info = basis::DeserializeFromSpan<proto::NetworkInfo>(complete->GetPayload());
        spdlog::info("Got network info message {}", last_network_info->DebugString());

        // convert to proto::PublisherInfo
        // fallthrough
      }
      case plugins::transport::TcpConnection::ReceiveStatus::DOWNLOADING: {
        break;
      }
      case plugins::transport::TcpConnection::ReceiveStatus::ERROR: {
        spdlog::error("connection error after bytes {} - got error {} {}",
                      in_progress_packet.GetCurrentProgress(), errno, strerror(errno));
        // fallthrough
      }
      case plugins::transport::TcpConnection::ReceiveStatus::DISCONNECTED: {
        // TODO: we can do a fast delete here instead, swapping with .last()
        break;
      }
      }
  }

  std::unique_ptr<proto::NetworkInfo> last_network_info;
    
  IncompleteMessagePacket in_progress_packet;

};
} // namespace basis::core::transport