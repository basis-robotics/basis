
#include <arpa/inet.h>
#include <basis/core/networking/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <basis/core/networking/logger.h>

namespace basis {
namespace core {
namespace networking {

Socket::Socket(int fd) : fd(fd) {}

Socket::~Socket() { Close(); }

void Socket::Close() { close(fd); }
int Socket::Send(const std::byte *data, size_t len) {
  if (fd == -1) {
    BASIS_LOG_CRITICAL("Trying to send() on an invalid socket");
  }
  return send(fd, data, len, MSG_NOSIGNAL);
}

int Socket::RecvInto(char *buffer, size_t buffer_len, bool peek) {
  // todo: error handling + close
  return recv(fd, buffer, buffer_len, peek ? MSG_PEEK : 0);
}

std::optional<Socket::Error> Socket::Select(bool send, int timeout_s, int timeout_ns) {
  struct timeval tv;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  tv.tv_sec = timeout_s;
  tv.tv_usec = timeout_ns;

  int select_results =  send 
    ? select(fd + 1, /*read*/ NULL, /*write*/ &fds, /*except*/ NULL, &tv)
    : select(fd + 1, /*read*/ &fds, /*write*/ NULL, /*except*/ NULL, &tv);
  if (select_results == 0) {
    // TODO: double check errno values in timeout
    return Error{ErrorSource::TIMEOUT, 0};
  } else if (select_results == -1) {
    return Error{ErrorSource::SELECT, errno};
  }

  return {};
}

void Socket::SetNonblocking() {
  /// @todo error handling
  int flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

nonstd::expected<TcpSocket, Socket::Error> TcpSocket::Connect(std::string_view address, uint16_t port) {
  struct addrinfo hints, *res;
  int sockfd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // allow ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM;

  int ret = getaddrinfo(std::string{address}.c_str(), std::to_string(port).c_str(), &hints, &res);
  if (ret != 0) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::GETADDRINFO, ret});
  }

  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd == -1) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::SOCKET, errno});
  }

  ret = connect(sockfd, res->ai_addr, res->ai_addrlen);
  if (ret == -1) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::CONNECT, errno});
  }

  return TcpSocket(sockfd);
}

void TcpSocket::TcpNoDelay() {
  // Enforce TCP_NODELAY by default
  int one = 1;
  setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
}

uint16_t TcpListenSocket::GetPort() const {
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  if (getsockname(fd, (struct sockaddr *)&sin, &len) == -1) {
    return 0;
  } else {
    return ntohs(sin.sin_port);
  }
}
nonstd::expected<TcpListenSocket, Socket::Error> TcpListenSocket::Create(uint16_t port) {
  struct addrinfo hints, *res;
  int sockfd;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // allow ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int ret = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res);
  if (ret != 0) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::GETADDRINFO, ret});
  }

  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd == -1) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::SOCKET, errno});
  }

  int yes = 1;
  ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (ret == -1) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::SETSOCKOPT, errno});
  }

  // bind it to the port we passed in to getaddrinfo():
  ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
  if (ret != 0) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::BIND, errno});
  }

  if (listen(sockfd, max_backlog_connections) == -1) {
    return nonstd::make_unexpected(Socket::Error{Socket::ErrorSource::LISTEN, errno});
  }

  return TcpListenSocket(sockfd);
}

nonstd::expected<TcpSocket, Socket::Error> TcpListenSocket::Accept(int timeout_s) {
  sockaddr_storage addr{};
  socklen_t addr_size = sizeof(addr);

  if (timeout_s >= 0) {
    auto error = Select(false, timeout_s, 0);
    if (error) {
      return nonstd::make_unexpected(*error);
    }
  }
  int client_fd = accept4(fd, (struct sockaddr *)&addr, &addr_size, O_CLOEXEC);
  if (client_fd == -1) {
    return nonstd::make_unexpected(Socket::Error{ErrorSource::ACCEPT, errno});
  }
  return client_fd;
}

} // namespace networking
} // namespace core
} // namespace basis