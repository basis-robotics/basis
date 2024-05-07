#include <memory>
#include <thread>

#include <gtest/gtest.h>
#include <basis/plugins/transport/tcp.h>

using namespace basis::plugins::transport;
using namespace basis::core::networking;

TEST(TcpTransport, NoCoordinator) {
    auto maybe_listen_socket = TcpListenSocket::Create(4242);
    ASSERT_TRUE(maybe_listen_socket.has_value());
    TcpListenSocket socket{std::move(maybe_listen_socket.value())};
    
    auto receiver = std::make_unique<TcpReceiver>("127.0.0.1", 4242);
    ASSERT_FALSE(receiver->IsConnected());
    receiver->Connect();
    ASSERT_TRUE(receiver->IsConnected());

    auto maybe_sender_socket = socket.Accept(1);
    ASSERT_TRUE(maybe_sender_socket.has_value());
    TcpSender sender(std::move(maybe_sender_socket.value()));
    ASSERT_TRUE(sender.IsConnected());

    const std::string message = "Hello, World!";
    sender.Send(message.c_str(), message.size() + 1);

    char buffer[1024];
    receiver->Receive(buffer, 1024, 1);

    ASSERT_STREQ(buffer, message.c_str());


//    using RawMessage = std::span<std::byte>;
    //RawMessage m(message.data(), message.size())
}