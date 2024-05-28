#pragma once

#include <basis/core/networking/socket.h>

#include <basis/core/transport/transport.h>

#include <memory>

namespace basis::plugins::transport {

/**
 * Class for unifying TcpSender and Receiver
 */
class TcpConnection {

protected:
    TcpConnection() {}
    TcpConnection(basis::core::networking::TcpSocket socket) : socket(std::move(socket)) {}
public:
  /// @todo this isn't strictly true. Need to store a flag on error, instead
  bool IsConnected() const { return socket.IsValid(); }

  /**
   *
   *
   * returns unique as it's expected a transport will handle this.
   * @todo why do we need to return unique? We have a unique ptr wrapping a unique ptr - unneccessary.
   */
  std::unique_ptr<const basis::core::transport::MessagePacket> ReceiveMessage(int timeout_s);

  // todo: standardized class for handling these
  enum class ReceiveStatus { DOWNLOADING, DONE, ERROR, DISCONNECTED };
  ReceiveStatus ReceiveMessage(basis::core::transport::IncompleteMessagePacket &message);

  /**
   * @todo error handling
   */
  bool Receive(std::byte *buffer, size_t buffer_len, int timeout_s = -1);

protected:
  basis::core::networking::TcpSocket socket;
};

}