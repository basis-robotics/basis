#include <basis/plugins/transport/tcp_connection.h>
#include <spdlog/spdlog.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <optional>
#include <span>

#include "basis/core/networking/socket.h"
#include "basis/core/transport/transport.h"
#include "spdlog/fmt/bundled/core.h"

namespace basis::plugins::transport {

std::unique_ptr<const basis::core::transport::MessagePacket> TcpConnection::ReceiveMessage(int timeout_s) {
  core::transport::MessageHeader header;
  if (!Receive((std::byte *)&header, sizeof(header), timeout_s)) {
    spdlog::error("ReceiveMessage failed to get header due to {} {}", errno, strerror(errno));
    spdlog::error("Failed to get header");
    return {};
  }

  auto message = std::make_unique<core::transport::MessagePacket>(header);
  std::span<std::byte> payload(message->GetMutablePayload());
  if (!Receive(payload.data(), payload.size(), timeout_s)) {
    spdlog::error("ReceiveMessage failed to get payload due to {} {}", errno, strerror(errno));

    spdlog::error("Failed to get payload");
    return {};
  }

  return message;
}

TcpConnection::ReceiveStatus TcpConnection::ReceiveMessage(core::transport::IncompleteMessagePacket &incomplete) {
  spdlog::trace("TcpConnection::ReceiveMessage");
  int count = 0;
  do {
    std::span<std::byte> buffer = incomplete.GetCurrentBuffer();

    // Download some bytes
    spdlog::trace("RecvInto {}", buffer.size());
    count = socket.RecvInto((char *)buffer.data(), buffer.size());

    if (count < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        spdlog::error("ReceiveMessage failed due to {} {}", errno, strerror(errno));
        return ReceiveStatus::ERROR;
      } else {
        spdlog::trace("ReceiveMessage EWOULDBLOCK");
      }
      return ReceiveStatus::DOWNLOADING;
    }
    if (count == 0) {
      return ReceiveStatus::DISCONNECTED;
    }
    if (count > 0) {
      spdlog::debug("ReceiveMessage Got {} bytes", count);
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
      if (socket.Select(timeout_s, 0)) {
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
      printf("Error: %i", errno);
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
    spdlog::trace("Sent {} bytes", sent_size);
    if (sent_size < 0) {
      return false;
    }
    len -= sent_size;
    data += sent_size;
  }

  return true;
}

} // namespace basis::plugins::transport