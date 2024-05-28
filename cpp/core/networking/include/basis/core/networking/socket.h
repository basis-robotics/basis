#pragma once

#include <nonstd/expected.hpp>
#include <tuple>

#include <optional>
#include <string>

namespace basis {
namespace core {
namespace networking {

/**
 * Wrapper around raw sockets, handling destruction and adding error handling.
 */
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

  /**
   * Checks if the Socket _could_ be a valid. Note that a valid fd could still be in a disconnected state.
   */
  bool IsValid() const { return fd != -1; }

  /**
   * Returns the underlying fd, for use with external APIs.
   *
   * It's recommended not to hold onto the result of this, unless you are ensuring that it doesn't survive past the
   * lifetime of the Socket.
   */
  int GetFd() const { return fd; }

protected:
  Socket(int fd = -1) : fd(fd) {}
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;
  Socket(Socket &&other) {
    Close();

    fd = other.fd;
    other.fd = -1;
  }
  Socket &operator=(Socket &&other) {
    Close();

    fd = other.fd;
    other.fd = -1;
    return *this;
  }

  virtual ~Socket();

  /**
   * Calls basic unix close()
   *
   * @todo add a flag to check if socket is valid to avoid race conditions and threading.
   * could we copy out the fd to a temporary, set to -1 and then close? that might satisfy ordering requirements
   */
  void Close();

public:
  /**
   * Sends data over the socket - returns the number of bytes sent, or -1 on error.
   * Currently it's on the caller to handle the error.
   *
   * @todo Make a proper error handling API here
   */
  int Send(const std::byte *data, size_t len);

  /**
   * select()
   *
   * @todo this should be moved out to a static to handle select on multiple sockets
   * @todo deprecate this in favor of poll based options
   */
  std::optional<Error> Select(int timeout_s, int timeout_ns);

  

  /**
   * Receives data into the requested buffer,
   *
   * @todo Make a proper error handling API here
   * @todo Use basis::core::Time
   */
  int RecvInto(char *buffer, size_t buffer_len, bool peek = false);

  void SetNonblocking();

protected:
  int fd{-1};
};

class TcpSocket : public Socket {
public:
  TcpSocket(int fd = -1 /* todo: pass in address info */) : Socket(fd) {
    // Enforce TcpNoDelay by default
    TcpNoDelay();
  }

  void TcpNoDelay();

  static nonstd::expected<TcpSocket, Socket::Error> Connect(std::string_view host, uint16_t port);
};

class TcpListenSocket : public Socket {
protected:
  TcpListenSocket(int fd) : Socket(fd) {}

public:
  static nonstd::expected<TcpListenSocket, Socket::Error> Create(uint16_t port);

  uint16_t GetPort() const;

  // TODO: time integration
  nonstd::expected<TcpSocket, Socket::Error> Accept(int timeout_s = -1);

private:
  static constexpr int max_backlog_connections = 20;
};

} // namespace networking
} // namespace core
} // namespace basis