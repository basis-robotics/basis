#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>

#include <expected>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>



#include <string.h>

namespace basis {
namespace plugins {
namespace transport {

class Socket {
    protected:
    Socket(int fd = -1): fd(fd) {
        
    }
    // Make not copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    //Allow move
    Socket(Socket&& other) = default;
    Socket& operator=(Socket&& other) = default;

    bool IsValid() {
        return fd != -1;
    }

    ~Socket() {
        close(fd);
    }
    int fd;
};

enum class SocketError {
    GETADDRINFO,
    SOCKET,
    BIND,
};


class TcpListenSocket : public Socket {
public:
    TcpListenSocket(int fd) : Socket(fd) {
 

    }
};

std::expected<TcpListenSocket, SocketError> CreateTcpListenSocket(uint16_t port) {
        struct addrinfo hints, *res;
        int sockfd;


        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

        getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res);

        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        // bind it to the port we passed in to getaddrinfo():

        bind(sockfd, res->ai_addr, res->ai_addrlen);

        return TcpListenSocket(sockfd);
}

struct ConnectionInfo {
    std::string address;
    uint16_t port;
};

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



} // namespace transport
} // namespace plugins
} // namespace basis