       #include <string.h>

#include <basis/plugins/transport/tcp.h>


namespace basis::plugins::transport {
    void TcpSender::StartThread() {
        printf("Starting thread\n");
        send_thread = std::thread([this]() {
            while(!stop_thread) {
                printf("loop\n");
                std::vector<std::shared_ptr<const core::transport::RawMessage>> buffer;
                {
                    std::unique_lock lock(send_mutex);
                    if(buffer.empty()) {
                        send_cv.wait(lock, [this] { return stop_thread || !send_buffer.empty(); });
                    }
                    buffer = std::move(send_buffer);   
                }
                if(stop_thread) {
                    printf("stop\n");
                    return;
                }

                
                for(auto& message : buffer) {
                    printf("sending message... '%s'\n", message->GetPacket().data() + sizeof(core::transport::MessageHeader));
                    std::span<const std::byte> packet = message->GetPacket();
                    if(!Send(packet.data(), packet.size())) {
                        printf("Stopping send thread due to %s\n", strerror(errno));
                        stop_thread = true;
                    }
                    printf("Sent\n");
                    if(stop_thread) {
                        printf("stop\n");
                        return;
                    }
                }
            }
        });
    }

    bool TcpSender::Send(const std::byte* data, size_t len) {
        while(len) {
            size_t sent_size = socket.Send(data, len);
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


    int TcpReceiver::Receive(char* buffer, size_t buffer_len, int timeout_s) {
        return socket.RecvInto(buffer, buffer_len, timeout_s);
    }
} // namespace basis::plugins::transport