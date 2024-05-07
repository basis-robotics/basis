#include <thread>

#include <gtest/gtest.h>
#include <basis/core/networking/socket.h>

using namespace basis::core::networking;

TEST(TcpListenSocket, TestAcceptTimeout) {
    auto maybe_listen_socket = TcpListenSocket::Create(4242);
    ASSERT_TRUE(maybe_listen_socket.has_value());
    TcpListenSocket socket{std::move(maybe_listen_socket.value())};
    ASSERT_NE(socket.GetFd(), -1);
    auto maybe_client = socket.Accept(1);
    ASSERT_FALSE(maybe_client.has_value());
    auto error = maybe_client.error();
    ASSERT_EQ(error.first, Socket::ErrorSource::TIMEOUT);
}

TEST(TcpListenSocket, TestAcceptSuccess) {
    auto maybe_listen_socket = TcpListenSocket::Create(4242);
    ASSERT_TRUE(maybe_listen_socket.has_value());
    TcpListenSocket socket{std::move(maybe_listen_socket.value())};
    ASSERT_NE(socket.GetFd(), -1);

    auto maybe_client_socket = TcpSocket::Connect("127.0.0.1", 4242);
    ASSERT_TRUE(maybe_client_socket.has_value());
    TcpSocket client_socket{std::move(maybe_client_socket.value())};

    auto maybe_server_socket = socket.Accept(1);
    ASSERT_TRUE(maybe_server_socket.has_value());
    TcpSocket server_socket{std::move(maybe_server_socket.value())};
    
    const std::string message = "Hello, World!";

    client_socket.Send(message.c_str(), message.size() + 1);

    char buffer[1024];

    server_socket.RecvInto(buffer, 1024);

    ASSERT_STREQ(buffer, message.c_str());

    const std::string response = "Goodbye, World!";

    memset(buffer, 0, 1024);

    server_socket.Send(response.c_str(), response.size() + 1);

    client_socket.RecvInto(buffer, 1024);

    ASSERT_STREQ(buffer, response.c_str());

}
