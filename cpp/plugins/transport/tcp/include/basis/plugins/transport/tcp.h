#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/transport.h>
#include <basis/core/networking/socket.h>


namespace basis::plugins::transport {

class TcpSender : public core::transport::TransportSender {
    TcpSender(std::string_view address)

    virtual int Send(const char* data, size_t len) override;
    
    TcpSocket socket;
};

class TcpReceiver : public core::transport::TransportReceiver {
    virtual int Receive(char* buffer, size_t buffer_len) override;

    TcpSocket socket;
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