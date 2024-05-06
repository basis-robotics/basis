#pragma once

#include <memory>

namespace basis::core::transport {
class TransportSender {
    virtual void Send(const char* data, size_t len) = 0;
};

class TransportReceiver {
    virtual int RecvInto(char* buffer, size_t buffer_len, int timeout_s) = 0;
};
#if 0
class Transport {
    static virtual std::unique_ptr<TransportPublisher*> Advertise(std::string_view topic) = 0;
    static virtual std::unique_ptr<TransportPublisher*> Subscribe(std::string_view topic) = 0;
    
    virtual void Send(const char* data, size_t len) = 0;
    virtual int RecvInto(char* buffer, size_t buffer_len, int timeout_s) = 0;
}
#endif
} // namespace basis::core::transport