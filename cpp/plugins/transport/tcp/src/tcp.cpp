#include <basis/plugins/transport/tcp.h>

namespace basis::plugins::transport {

    int TcpSender::Send(const char* data, size_t len) {
        return socket.Send(data, len);
    }

    int TcpReceiver::Receive(char* buffer, size_t buffer_len, int timeout_s) {
        return socket.RecvInto(buffer, buffer_len, timeout_s);
    }
} // namespace basis::plugins::transport