#include <memory>
#include <span>
#include <thread>

#include <gtest/gtest.h>
#include <basis/plugins/transport/tcp.h>

using namespace basis::core::networking;

namespace basis::plugins::transport {


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
    sender.Send((std::byte*)message.c_str(), message.size() + 1);

    char buffer[1024];
    receiver->Receive(buffer, 1024, 1);

    ASSERT_STREQ(buffer, message.c_str());

    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(basis::core::transport::MessageHeader::DataType::MESSAGE, message.size() + 1);
    strcpy((char*)shared_message->GetMutablePayload().data(), message.data());
    printf("Mutable string is %s\n", shared_message->GetMutablePayload().data());
    sender.SendMessage(shared_message);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    printf("Done sleeping\n");
    buffer[0] = 0;
    int rret = receiver->Receive(buffer, 1024, 1);
    printf("recv got %i bytes\n", rret);

    ASSERT_STREQ(buffer + sizeof(basis::core::transport::MessageHeader), message.c_str());

    core::transport::MessageHeader* header = reinterpret_cast<core::transport::MessageHeader*>(buffer);
    printf("Magic is %c %c %c %i\n", header->magic_version[0], header->magic_version[1], header->magic_version[2], header->magic_version[3]);
    ASSERT_EQ(memcmp(header->magic_version, std::array<char, 4>{'B', 'A', 'S', 0}.data(), 4), 0);
}
}