#pragma once

#include <tuple>
#include <expected>

#include <string>
#include <optional>


namespace basis {
namespace core {
namespace networking {

class Socket {
public:

    enum class ErrorSource {
        TIMEOUT,
        ACCEPT,
        GETADDRINFO,
        SOCKET,
        BIND,
        LISTEN,
        SELECT,
        CONNECT,
        SETSOCKOPT,
    };

    using Error = std::pair<Socket::ErrorSource, int>;


    bool IsValid() const {
        return fd != -1;
    }

    const int GetFd() {
        return fd;
    }

protected:
    Socket(int fd = -1): fd(fd) {
        
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) {
        Close();
        std::swap(fd, other.fd);
    }
    Socket& operator=(Socket&& other) {
        Close();
        std::swap(fd, other.fd);
        return *this;
    }
    

    void Close();

    ~Socket();

public:

    // TODO: should this be wrapped to handle partial sends?
    int Send(const std::byte* data, size_t len);

    // TODO: this should be moved out to a static to handle select on multiple sockets
    std::optional<Error> Select( int timeout_s, int timeout_ns);

    // TODO: error handling
    int RecvInto(char* buffer, size_t buffer_len, int timeout_s = -1);

protected:

    int fd {-1};

};

using SocketError = Socket::Error;



class TcpSocket : public Socket {
public:
    TcpSocket(int fd = -1 /* todo: pass in address info */) : Socket(fd) {
        
    }
     
    static std::expected<TcpSocket, SocketError> Connect(std::string_view host, uint16_t port);
};



class TcpListenSocket : public Socket {
protected:
    TcpListenSocket(int fd) : Socket(fd) { 

    }
public:
    static std::expected<TcpListenSocket, SocketError> Create(uint16_t port);

    // TODO: time integration
    std::expected<TcpSocket, SocketError> Accept(int timeout_s = -1);


private:
    static constexpr int max_backlog_connections = 20;
};



}
}
}