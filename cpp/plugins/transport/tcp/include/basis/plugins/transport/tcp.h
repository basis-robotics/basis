#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/transport.h>
#include <basis/core/networking/socket.h>


namespace basis::plugins::transport {

class TcpSender : public core::transport::TransportSender {
    public:
    TcpSender(core::networking::TcpSocket&& socket) : socket(std::move(socket)) {
        
    }

    bool IsConnected() {
        return socket.IsValid();
    }

    virtual int Send(const char* data, size_t len) override;
    private:
    core::networking::TcpSocket socket;

};

class TcpReceiver : public core::transport::TransportReceiver {
    public:
    TcpReceiver(std::string_view address, uint16_t port) : address(address), port(port) {
        
    }

    virtual bool Connect() {
        auto maybe_socket = core::networking::TcpSocket::Connect(address, port);
        if(maybe_socket) {
            socket = std::move(maybe_socket.value());
            return true;
        }
        return false;
    }
    bool IsConnected() {
        return socket.IsValid();
    }

    virtual int Receive(char* buffer, size_t buffer_len, int timeout_s) override;
private:
    core::networking::TcpSocket socket;

    std::string address;
    uint16_t port;
};


#if 0
// TODO: why does this need to be typed?

template<typename T_MSG>
class TcpPublisher : public core::transport::PublisherBaseT<T_MSG> {
public:
    TcpPublisher(std::string_view topic) : topic(topic) {}

    virtual void Publish(const T_MSG& msg) override {
        
    }

    void Publish(const char* data, size_t size) {
        
    }


private:
    std::string topic;
};

template<typename T_MSG>
class TcpSubscriber : public core::transport::SubscriberBaseT<T_MSG> {
    // TODO: buffer size
public:
    TcpSubscriber(const std::function<void(const core::transport::MessageEvent<T_MSG>& message)> callback) : core::transport::SubscriberBaseT<T_MSG>(callback) {}

    virtual void OnMessage(std::shared_ptr<const T_MSG> msg) override {
        
    }

    virtual void ConsumeMessages(const bool wait = false) override {
      
    }

    
};

#endif

} // namespace basis::plugins::transport