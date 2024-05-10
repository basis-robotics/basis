#include <string.h>

#include <basis/plugins/transport/tcp.h>

#include <spdlog/spdlog.h>


namespace basis::plugins::transport {
    void TcpSender::StartThread() {
        spdlog::info("Starting TcpSender thread\n");
        send_thread = std::thread([this]() {
            while(!stop_thread) {
                std::vector<std::shared_ptr<const core::transport::RawMessage>> buffer;
                {
                    std::unique_lock lock(send_mutex);
                    if(buffer.empty()) {
                        send_cv.wait(lock, [this] { return stop_thread || !send_buffer.empty(); });
                    }
                    buffer = std::move(send_buffer);   
                }
                if(stop_thread) {
                    return;
                }

                for(auto& message : buffer) {
                    spdlog::trace("sending message... '{}'", (char*)(message->GetPacket().data() + sizeof(core::transport::MessageHeader)));
                    std::span<const std::byte> packet = message->GetPacket();
                    if(!Send(packet.data(), packet.size())) {
                        spdlog::trace("Stopping send thread due to {}: {}", errno, strerror(errno));
                        stop_thread = true;
                    }
                    spdlog::trace("Sent");
                    if(stop_thread) {
                        spdlog::trace("stop TcpSender");
                        return;
                    }
                }
            }
        });
    }
    
    bool TcpSender::Send(const std::byte* data, size_t len) {
        // TODO: this loop should go on a helper on Socket
        while(len) {
            int sent_size = socket.Send(data, len);
            if(sent_size < 0) {
                return false;
            }
            len -= sent_size;
            data += sent_size;
        }

        return true;
    }

    void TcpSender::SendMessage(std::shared_ptr<core::transport::RawMessage> message) {
        std::lock_guard lock(send_mutex);
        send_buffer.emplace_back(std::move(message));
        send_cv.notify_one();
    }

    bool TcpReceiver::Receive(std::byte* buffer, size_t buffer_len, int timeout_s) {
        while(buffer_len) {
            int recv_size = socket.RecvInto((char*)buffer, buffer_len, timeout_s);
            // timeout
            // todo: error handling
            if(recv_size == 0) {
                return false;
            }
            if(recv_size < 0) {
                printf("Error: %i", errno);
                // should disconnect here
                return false;
            }
            buffer += recv_size;
            buffer_len -= recv_size;
        }
        return true; 
    }
    
    std::unique_ptr<const core::transport::RawMessage> TcpReceiver::ReceiveMessage(int timeout_s) {
        core::transport::MessageHeader header;
        if(!Receive((std::byte*)&header, sizeof(header), timeout_s)) {
            spdlog::error("ReceiveMessage failed to get header due to {} {}", errno, strerror(errno));
            spdlog::error("Failed to get header");
            return {};
        }

        auto message = std::make_unique<core::transport::RawMessage> (header);
        std::span<std::byte> payload(message->GetMutablePayload());
        if(!Receive(payload.data(), payload.size(), timeout_s)) {
            spdlog::error("ReceiveMessage failed to get payload due to {} {}", errno, strerror(errno));

            spdlog::error("Failed to get payload");
            return {};
        }

        return message;
    }
    
    bool TcpReceiver::ReceiveMessage(core::transport::IncompleteRawMessage& incomplete) {
        spdlog::info("TcpReceiver::ReceiveMessage");
        int count = 0;
        do {
            std::span<std::byte> buffer = incomplete.GetCurrentBuffer();

            // Download some bytes
            count = socket.RecvInto((char*)buffer.data(), buffer.size());
            if(count < 0) {
             //   if(errno != EAGAIN && errno != EWOULDBLOCK) {
                    spdlog::info("ReceiveMessage failed due to {} {}", errno, strerror(errno));
               // }
                // todo: this needs to return the error type
                return false;
            }
            if(count == 0) {
                spdlog::error("ReceiveMessage {} {}", errno, strerror(errno));
                return false;
            }
            if(count > 0) {
                spdlog::info("ReceiveMessage Got {} bytes", count);
            }
            // todo: handle EAGAIN
        // Continue downloading until we've gotten the whole message
        } while(!incomplete.AdvanceCounter(count));

        return true;
    }

} // namespace basis::plugins::transport