#include <basis/plugins/transport/tcp_connection.h>

#include <spdlog/spdlog.h>

#include <basis/plugins/transport/logger.h>

DECLARE_AUTO_LOGGER_PLUGIN(transport, tcp)

namespace basis::plugins::transport {

using namespace tcp;

std::unique_ptr<const basis::core::transport::MessagePacket> TcpConnection::ReceiveMessage(int timeout_s) {
  core::transport::MessageHeader header;
  if (!Receive((std::byte *)&header, sizeof(header), timeout_s)) {
    BASIS_LOG_ERROR("ReceiveMessage failed to get header due to {} {}", errno, strerror(errno));
    BASIS_LOG_ERROR("Failed to get header");
    return {};
  }

  auto message = std::make_unique<core::transport::MessagePacket>(header);
  std::span<std::byte> payload(message->GetMutablePayload());
  if (!Receive(payload.data(), payload.size(), timeout_s)) {
    BASIS_LOG_ERROR("ReceiveMessage failed to get payload due to {} {}", errno, strerror(errno));

    BASIS_LOG_ERROR("Failed to get payload");
    return {};
  }

  return message;
}

TcpConnection::ReceiveStatus TcpConnection::ReceiveMessage(core::transport::IncompleteMessagePacket &incomplete) {
  BASIS_LOG_TRACE("TcpConnection::ReceiveMessage");
  int count = 0;
  do {
    std::span<std::byte> buffer = incomplete.GetCurrentBuffer();

    // Download some bytes
    BASIS_LOG_TRACE("RecvInto {}", buffer.size());
    count = socket.RecvInto((char *)buffer.data(), buffer.size());

    if (count < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        BASIS_LOG_ERROR("ReceiveMessage failed due to {} {}", errno, strerror(errno));
        return ReceiveStatus::ERROR;
      } else {
        BASIS_LOG_TRACE("ReceiveMessage EWOULDBLOCK");
      }
      return ReceiveStatus::DOWNLOADING;
    }
    if (count == 0) {
      return ReceiveStatus::DISCONNECTED;
    }
    if (count > 0) {
      BASIS_LOG_DEBUG("ReceiveMessage Got {} bytes", count);
    }
    // todo: handle EAGAIN
    // Continue downloading until we've gotten the whole message
  } while (!incomplete.AdvanceCounter(count));

  return ReceiveStatus::DONE;
}

bool TcpConnection::Receive(std::byte *buffer, size_t buffer_len, int timeout_s) {
  while (buffer_len) {
    if (timeout_s >= 0) {
      // todo: error handling
      if (socket.Select(false, timeout_s, 0)) {
        return false;
      }
    }
    int recv_size = socket.RecvInto((char *)buffer, buffer_len);
    // timeout
    // todo: error handling
    if (recv_size == 0) {
      return false;
    }
    if (recv_size < 0) {
      BASIS_LOG_ERROR("TcpConnection::Receive Error: %i", errno);
      // should disconnect here
      return false;
    }
    buffer += recv_size;
    buffer_len -= recv_size;
  }
  return true;
}

bool TcpConnection::Send(const std::byte *data, size_t len) {
  // TODO: this loop should go on a helper on Socket(?)
  while (len) {
    int sent_size = socket.Send(data, len);
    BASIS_LOG_TRACE("TCP Sent {} bytes", sent_size);
    if (sent_size < 0) {
      // Very large sends can cause EAGAIN if we fill the send buffer
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
        socket.Select(true, 0, 0);
        continue;
      }

      BASIS_LOG_ERROR("TcpConnection::Send Error: {}", errno);      
      return false;
    }
    len -= sent_size;
    data += sent_size;
  }

  return true;
}

} // namespace basis::plugins::transport