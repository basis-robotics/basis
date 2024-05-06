#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/publisher.h>

#include <expected>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
       #include <fcntl.h>



#include <string.h>

namespace basis {
namespace plugins {
namespace transport {

class Socket {
    protected:
    Socket(int fd = -1): fd(fd) {
        
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
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

enum class SocketErrorFunction {
    TIMEOUT,
    GETADDRINFO,
    SOCKET,
    BIND,
};

class TcpSocket : public Socket {
public:
    TcpSocket(int fd /* todo: pass in address info */) : Socket(fd) {
        
    }

};


using SocketError = std::pair<SocketErrorFunction, int>;


class TcpListenSocket : public Socket {
public:
    TcpListenSocket(int fd) : Socket(fd) { 

    }

    std::expected<TcpSocket, SocketError> Accept() {
        sockaddr_storage addr;
        socklen_t addr_size = sizeof(addr);
        int client_fd = accept4(fd, (struct sockaddr *)&addr, &addr_size, O_CLOEXEC);
        if(client_fd == -1) {
            return std::unexpected(SocketError{SocketErrorFunction::TIMEOUT, errno});
        }
        return TcpSocket(client_fd);
    }
};

std::expected<TcpListenSocket, SocketError> CreateTcpListenSocket(uint16_t port) {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // allow ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int ret = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res);
    if(ret != 0) {
        return std::unexpected(SocketError{SocketErrorFunction::GETADDRINFO, ret});
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == -1) {
        return std::unexpected(SocketError{SocketErrorFunction::SOCKET, errno});
    }

    // bind it to the port we passed in to getaddrinfo():
    ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if(ret != 0) {
        return std::unexpected(SocketError{SocketErrorFunction::BIND, errno});
    }

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