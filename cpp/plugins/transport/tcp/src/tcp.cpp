#include <string.h>

#include <basis/plugins/transport/tcp.h>

#include <spdlog/spdlog.h>

namespace basis::plugins::transport {
using namespace tcp;
void TcpSender::StartThread() {
  BASIS_LOG_TRACE("Starting TcpSender thread");

  send_thread = std::thread([this]() {
    while (!stop_thread) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
        BASIS_LOG_TRACE("Sending a message of size {}", message->GetPacket().size());
        std::span<const std::byte> packet = message->GetPacket();
        if (!Send(packet.data(), packet.size())) {
          BASIS_LOG_TRACE("Stopping send thread due to {}: {}", errno, strerror(errno));
          stop_thread = true;
        }
        BASIS_LOG_TRACE("Sent");
        if (stop_thread) {
          BASIS_LOG_TRACE("stop TcpSender");
          return;
        }
      }
    }
  });
}

void TcpSender::SetMaxQueueSize(size_t max_queue_size) {
  this->max_queue_size = max_queue_size;

  if (max_queue_size > 0) {
    std::lock_guard lock(send_mutex);
    while (send_buffer.size() >= max_queue_size)
      send_buffer.erase(send_buffer.begin());
  }
}

void TcpSender::SendMessage(std::shared_ptr<core::transport::MessagePacket> message) {
  BASIS_LOG_TRACE("Queueing a message of size {}", message->GetPacket().size());
  std::lock_guard lock(send_mutex);

  if (max_queue_size > 0) {
    if (send_buffer.size() >= max_queue_size) {
      BASIS_LOG_INFO("TcpSender::SendMessage trimming queue {} -> {}", send_buffer.size() + 1, max_queue_size);
    }

    while (send_buffer.size() >= max_queue_size)
      send_buffer.erase(send_buffer.begin());
  }

  send_buffer.emplace_back(std::move(message));
  send_cv.notify_one();
}

nonstd::expected<std::shared_ptr<TcpPublisher>, core::networking::Socket::Error> TcpPublisher::Create(uint16_t port) {
  BASIS_LOG_DEBUG("Create TcpListenSocket");
  auto maybe_listen_socket = core::networking::TcpListenSocket::Create(port);
  if (!maybe_listen_socket) {
    return nonstd::make_unexpected(maybe_listen_socket.error());
  }

  return std::shared_ptr<TcpPublisher>(new TcpPublisher(std::move(maybe_listen_socket.value())));
}

TcpPublisher::TcpPublisher(core::networking::TcpListenSocket listen_socket) : listen_socket(std::move(listen_socket)) {}

uint16_t TcpPublisher::GetPort() { return listen_socket.GetPort(); }

void TcpPublisher::SetMaxQueueSize(size_t max_queue_size) {
  this->max_queue_size = max_queue_size;
  for (auto &sender : senders) {
    sender->SetMaxQueueSize(max_queue_size);
  }
}

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
    auto sender = std::make_unique<TcpSender>(std::move(maybe_sender_socket.value()));
    sender->SetMaxQueueSize(max_queue_size);
    senders.emplace_back(std::move(sender));
    num++;
  }
  return num;
}

} // namespace basis::plugins::transport
