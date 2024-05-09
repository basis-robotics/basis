#pragma once

#include <thread>
#include <vector>
#include <condition_variable>

#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/transport.h>
#include <basis/core/networking/socket.h>

namespace basis::plugins::transport {


/**
 * Used to send serialized data over TCP.
 * 
 * Currently spawns thread per publisher. This should be performant but will waste memory in stack allocations.
 *
 * @todo Move to using a worker/thread pool - should be relatively easy.
 */
class TcpSender : public core::transport::TransportSender {
public:
    TcpSender(core::networking::TcpSocket&& socket) : socket(std::move(socket)) {
        StartThread();
    }

    ~TcpSender() {
        // TODO: do _not_ manually call Close() here - either switch to nonblocking mode or signal the send thread.
        // Doing otherwise could lead to race conditions.
        // socket.Close();
        Stop(true);
    }

    bool IsConnected() {
        // TODO: this does not handle failure cases - needs to query socket internally for validity
        return socket.IsValid();
    }

    // TODO: do we want to be able to send high priority packets?
    virtual void SendMessage(std::shared_ptr<core::transport::RawMessage> message) override;


    void Stop(bool wait = false) {
        stop_thread = true;
        
        send_cv.notify_one();
        if(wait) {
            if(send_thread.joinable()) {
                send_thread.join();
            }
        }
    }

protected:
    friend class TestTcpTransport;
    virtual bool Send(const std::byte* data, size_t len) override;
private:
    void StartThread();

    core::networking::TcpSocket socket;

    std::thread send_thread;
    std::condition_variable send_cv;
    std::mutex send_mutex;
    std::vector<std::shared_ptr<const core::transport::RawMessage>> send_buffer;
    std::atomic<bool> stop_thread = false;
};

/**
 * Used to receive serialized data over TCP.
 * 
 * @todo these could be pooled. If multiple subscribers to the same topic are created, we should only have to recieve once.
 * It's a bit of an early optimization, though.
 */
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

    bool IsConnected() const {
        return socket.IsValid();
    }

    /**
     * 
     *
     * returns unique as it's expected a transport will handle this
     */
    std::unique_ptr<const core::transport::RawMessage> ReceiveMessage(int timeout_s);
    
    bool ReceiveMessage(core::transport::IncompleteRawMessage& message);

    /**
    * @todo error handling
     */
    virtual bool Receive(std::byte* buffer, size_t buffer_len, int timeout_s = -1) override;

    const core::networking::Socket& GetSocket() const {
        return socket;
    }
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