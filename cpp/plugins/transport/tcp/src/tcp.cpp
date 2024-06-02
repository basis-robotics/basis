#include <string.h>
#include <basis/plugins/transport/tcp.h>
#include <spdlog/spdlog.h>
#include <errno.h>
#include <stdint.h>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include "basis/core/networking/socket.h"
#include "basis/core/transport/message_packet.h"
#include "nonstd/expected.hpp"
#include "spdlog/fmt/bundled/core.h"

namespace basis::plugins::transport {
void TcpSender::StartThread() {
  spdlog::trace("Starting TcpSender thread");

  send_thread = std::thread([this]() {
    while (!stop_thread) {
      std::vector<std::shared_ptr<const core::transport::MessagePacket>> buffer;
      {
        std::unique_lock lock(send_mutex);
        if (!stop_thread && buffer.empty()) {
          send_cv.wait(lock, [this] { return stop_thread || !send_buffer.empty(); });
        }
        buffer = std::move(send_buffer);
      }
      if (stop_thread) {
        return;
      }

      for (auto &message : buffer) {
        spdlog::trace("Sending a message of size {}", message->GetPacket().size());
        std::span<const std::byte> packet = message->GetPacket();
        if (!Send(packet.data(), packet.size())) {
          spdlog::trace("Stopping send thread due to {}: {}", errno, strerror(errno));
          stop_thread = true;
        }
        spdlog::trace("Sent");
        if (stop_thread) {
          spdlog::trace("stop TcpSender");
          return;
        }
      }
    }
  });
}

void TcpSender::SendMessage(std::shared_ptr<core::transport::MessagePacket> message) {
  spdlog::trace("Queueing a message of size {}", message->GetPacket().size());

  std::lock_guard lock(send_mutex);
  send_buffer.emplace_back(std::move(message));
  send_cv.notify_one();
}

nonstd::expected<std::shared_ptr<TcpPublisher>, core::networking::Socket::Error> TcpPublisher::Create(uint16_t port) {
  spdlog::debug("Create TcpListenSocket");
  auto maybe_listen_socket = core::networking::TcpListenSocket::Create(port);
  if (!maybe_listen_socket) {
    return nonstd::make_unexpected(maybe_listen_socket.error());
  }

  return std::shared_ptr<TcpPublisher>(new TcpPublisher(std::move(maybe_listen_socket.value())));
}

TcpPublisher::TcpPublisher(core::networking::TcpListenSocket listen_socket) : listen_socket(std::move(listen_socket)) {}

uint16_t TcpPublisher::GetPort() { return listen_socket.GetPort(); }

void TcpPublisher::SendMessage(std::shared_ptr<core::transport::MessagePacket> message) {
  std::lock_guard lock(senders_mutex);
  for (auto &sender : senders) {
    sender->SendMessage(message);
  }
}

size_t TcpPublisher::CheckForNewSubscriptions() {
  int num = 0;

  while (auto maybe_sender_socket = listen_socket.Accept(0)) {
    std::lock_guard lock(senders_mutex);
    senders.emplace_back(std::make_unique<TcpSender>(std::move(maybe_sender_socket.value())));
    num++;
  }
  return num;
}

} // namespace basis::plugins::transport