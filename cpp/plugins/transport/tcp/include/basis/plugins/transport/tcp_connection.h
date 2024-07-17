#pragma once

#include <basis/core/networking/socket.h>

#include <basis/core/transport/transport.h>

#include <memory>

#include <basis/plugins/transport/logger.h>

namespace basis::plugins::transport {

/**
 * Common class for unifying TcpSender and Receiver functionality.
 * 
 * @todo send and recv should take a span - pointer + size isn't great.
 */
class TcpConnection {

protected:
    TcpConnection() {}

    explicit TcpConnection(basis::core::networking::TcpSocket socket) : socket(std::move(socket)) {
      this->socket.SetNonblocking();
    }
public:
  /**
   * Checks if this socket is likely connection. The state is 
   * 
   * @todo this isn't strictly true, due to socket not having an error flag.
   */
  bool IsConnected() const { return socket.IsValid(); }

  /**
   * Receives an entire message at once, blocking until complete or error.
   * It's recommended to not use this outside of test code.
   *
   * @todo why do we need to return unique? We have a unique ptr wrapping a unique ptr - unneccessary.
   */
  std::unique_ptr<const basis::core::transport::MessagePacket> ReceiveMessage(int timeout_s);

  /**
   * State enum for handling partial message receives.
   */
  enum class ReceiveStatus {
    /// This message is in progress
    DOWNLOADING, 
    /// This message is done - please extract it
    DONE, 
    /// An error occured
    ERROR, 
    /// A disconnection occured
    DISCONNECTED
  };

  /**
   * Receives as much data for a message as the underlying socket has.
   * 
   * @todo There's a fair amount of duplicated code for dealing with ReceiveStatus - we should be able to wrap it in a common handler.
   */
  ReceiveStatus ReceiveMessage(basis::core::transport::IncompleteMessagePacket &message);

  /**
   * Receives data over the socket into the buffer pointed to by `buffer`
   * Will not return until buffer_len bytes have been received or a disconnect occurs.
   *
   * @todo error handling
   */
  bool Receive(std::byte *buffer, size_t buffer_len, int timeout_s = -1);

  /**
   * Sends the data pointed at in the buffer over the socket.
   * 
   * @param data 
   * @param len 
   * @return bool 
   */
  bool Send(const std::byte *data, size_t len);

protected:
  /**
   * The underlying socket for this connection.
   */
  basis::core::networking::TcpSocket socket;
};

}