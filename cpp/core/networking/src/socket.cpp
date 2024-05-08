
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>

#include <basis/core/networking/socket.h>
namespace basis {
namespace core {
namespace networking {


Socket::~Socket() {
    Close();
}

void Socket::Close() {
    close(fd);
}
int Socket::Send(const std::byte* data, size_t len) {
    return send(fd, data, len, 0);
}

int Socket::RecvInto(char* buffer, size_t buffer_len, int timeout_s) {

    if(timeout_s >= 0) {
        auto error = Select(timeout_s, 0);
        if(error) {
            return 0;
        }
    }

    return recv(fd, buffer, buffer_len, 0);
}


std::optional<Socket::Error> Socket::Select(int timeout_s, int timeout_ns) {
    int resuolt;
    struct timeval tv;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    tv.tv_sec = timeout_s;
    tv.tv_usec = timeout_ns;

    int select_results = select(fd + 1, &rfds, (fd_set *) 0, (fd_set *) 0, &tv);
    if(select_results == 0) {
        // TODO: this isn't right
        return Error{ErrorSource::TIMEOUT, errno};
    } else if(select_results == -1) {
        return Error{ErrorSource::SELECT, errno};
    }

    return {};
}



std::expected<TcpSocket, Socket::Error> TcpSocket::Connect(std::string_view address, uint16_t port) {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // allow ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(std::string{address}.c_str(), std::to_string(port).c_str(), &hints, &res);
    if(ret != 0) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::GETADDRINFO, ret});
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == -1) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::SOCKET, errno});
    }

    ret = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if(ret == -1) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::CONNECT, errno});
    }

    return TcpSocket(sockfd);

}

std::expected<TcpListenSocket, Socket::Error> TcpListenSocket::Create(uint16_t port) {
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // allow ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int ret = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res);
    if(ret != 0) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::GETADDRINFO, ret});
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd == -1) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::SOCKET, errno});
    }
    
    int yes = 1;
    ret = setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    if(ret == -1) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::SETSOCKOPT, errno});
    }


    // bind it to the port we passed in to getaddrinfo():
    ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if(ret != 0) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::BIND, errno});
    }

    if(listen(sockfd, max_backlog_connections) == -1) {
        return std::unexpected(Socket::Error{Socket::ErrorSource::LISTEN, errno});
    }

    return TcpListenSocket(sockfd);
}

std::expected<TcpSocket, Socket::Error> TcpListenSocket::Accept(int timeout_s) {
    sockaddr_storage addr {};
    socklen_t addr_size = sizeof(addr);

    if(timeout_s >= 0) {
        auto error = Select(timeout_s, 0);
        if(error) {
            return std::unexpected(*error);
        }
    }
    int client_fd = accept4(fd, (struct sockaddr *)&addr, &addr_size, O_CLOEXEC);
    if(client_fd == -1) {
        return std::unexpected(Socket::Error{ErrorSource::ACCEPT, errno});
    }
    return std::move(TcpSocket(client_fd));
}

} // namespace networking
} // namespace core
} // namespace basis