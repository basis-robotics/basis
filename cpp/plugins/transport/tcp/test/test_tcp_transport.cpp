#include <memory>
#include <span>
#include <thread>

#include "spdlog/cfg/env.h"
#include <basis/plugins/transport/epoll.h>
#include <basis/plugins/transport/simple_mpsc.h>

#include <basis/plugins/transport/tcp.h>
#include <gtest/gtest.h>

#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <queue>

/*
void init_logger(){


    const std::string filename;
    int size = 10*1024*1024; // 10M
    int backcount = 5;       // 5

    // create console_sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    // sink's bucket
    spdlog::sinks_init_list sinks{console_sink};

    // create async logger, and use global threadpool
    spdlog::init_thread_pool(1024 * 8, 1);
    auto logger = std::make_shared<spdlog::async_logger>("aslogger", sinks, spdlog::thread_pool());
    spdlog::set_default_logger(logger);

}
*/

using namespace basis::core::networking;

namespace basis::plugins::transport {

class TestTcpTransport : public testing::Test {
public:
  TestTcpTransport() { spdlog::set_level(spdlog::level::debug); }

  TcpListenSocket CreateListenSocket(uint16_t port = 0) {
    spdlog::debug("Create TcpListenSocket");
    auto maybe_listen_socket = TcpListenSocket::Create(port);
    EXPECT_TRUE(maybe_listen_socket.has_value()) << port;
    return std::move(maybe_listen_socket.value());
  }

  std::unique_ptr<TcpReceiver> SubscribeToPort(uint16_t port) {
    spdlog::debug("Construct TcpReceiver");
    auto receiver = std::make_unique<TcpReceiver>("127.0.0.1", port);
    EXPECT_FALSE(receiver->IsConnected());
    receiver->Connect();

    spdlog::debug("Connect TcpReceiver to listen socket");
    EXPECT_TRUE(receiver->IsConnected());
    return receiver;
  }

  std::unique_ptr<TcpSender> AcceptOneKnownClient(TcpListenSocket &listen_socket) {
    spdlog::debug("Check for new connections via Accept");
    auto maybe_sender_socket = listen_socket.Accept(-1);
    EXPECT_TRUE(maybe_sender_socket.has_value());
    auto sender = std::make_unique<TcpSender>(std::move(maybe_sender_socket.value()));
    EXPECT_TRUE(sender->IsConnected());
    return sender;
  }

  /**
   * friend-ness isn't inherited, so make a helper here for the tests
   */
  bool Send(TcpSender &sender, const std::byte *data, size_t len) { return sender.Send(data, len); }
};

/**
 * Test raw send/recv with a single pair of sockets
 */
TEST_F(TestTcpTransport, NoCoordinator) {
  TcpListenSocket listen_socket = CreateListenSocket();
  uint16_t port = listen_socket.GetPort();
  ASSERT_NE(port, 0);
  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);
  std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

  spdlog::debug("Send raw bytes");
  const std::string hello = "Hello, World!";
  Send(*sender, (std::byte *)hello.c_str(), hello.size() + 1);

  spdlog::debug("Receive raw bytes");
  char buffer[1024];
  receiver->Receive((std::byte *)buffer, hello.size() + 1, 1);
  ASSERT_STREQ(buffer, hello.c_str());
  memset(buffer, 0, sizeof(buffer));

  spdlog::debug("Construct and send proper message packet");
  auto shared_message = std::make_shared<basis::core::transport::RawMessage>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());
  sender->SendMessage(shared_message);

  spdlog::debug("Sleep for a bit to allow the message to send");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  spdlog::debug("Done sleeping");

  spdlog::debug("Get the message");
  auto msg = receiver->ReceiveMessage(1.0);
  ASSERT_NE(msg, nullptr);

  spdlog::debug("Validate the header");
  const core::transport::MessageHeader *header = msg->GetMessageHeader();
  spdlog::debug("Magic is {}{}", std::string_view((char *)header->magic_version, 3), header->magic_version[3]);
  ASSERT_EQ(memcmp(header->magic_version, std::array<char, 4>{'B', 'A', 'S', 0}.data(), 4), 0);
  ASSERT_EQ(header->data_size, hello.size() + 1);
  ASSERT_STREQ((const char *)msg->GetPayload().data(), hello.c_str());

  sender->SendMessage(shared_message);
  sender->SendMessage(shared_message);
  sender->SendMessage(shared_message);

  ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
  ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
  ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
  // we've run out of messages
  ASSERT_EQ(receiver->ReceiveMessage(1.0), nullptr);
  sender->SendMessage(shared_message);
  ASSERT_NE(receiver->ReceiveMessage(1.0), nullptr);
}

/**
 * Test creating a publisher.
 */
TEST_F(TestTcpTransport, TestPublisher) {
  auto publisher = TcpPublisher::Create();
  ASSERT_TRUE(publisher.has_value());
  uint16_t port = publisher->GetPort();
  ASSERT_NE(port, 0);
  auto publish_over_port = TcpPublisher::Create(port);
  spdlog::debug("Successfully created publisher on port {}", port);
  ASSERT_FALSE(publish_over_port.has_value());
  int error = publish_over_port.error().second;
  spdlog::debug("Failed to create another publisher on same port - got [{}: {}]", strerrorname_np(error),
                strerror(error));

  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);

  ASSERT_EQ(publisher->CheckForNewSubscriptions(), 1);
}

/**
 * Test creationg a transport
 */
TEST_F(TestTcpTransport, TestTransport) {
  auto thread_pool_manager = std::make_shared<core::transport::ThreadPoolManager>();
  TcpTransport transport(thread_pool_manager);
  std::shared_ptr<core::transport::TransportPublisher> publisher = transport.Advertise("test");
  ASSERT_NE(publisher, nullptr);
}

struct TestStruct {
  uint32_t foo = 3;
  float bar = 8.5;
  std::string baz = "baz";
};

/**
 * Test full pipeline with transport manager
 */
TEST_F(TestTcpTransport, TestWithManager) {
  auto thread_pool_manager = std::make_shared<core::transport::ThreadPoolManager>();
  core::transport::TransportManager transport_manager;
  transport_manager.RegisterTransport("net_tcp", std::make_unique<TcpTransport>(thread_pool_manager));

  auto test_publisher = transport_manager.Advertise<TestStruct>("test_struct");
  ASSERT_NE(test_publisher, nullptr);

  //auto string_publisher = transport_manager.Advertise("test_string");
  //ASSERT_NE(publisher, nullptr);
}

/**
 * Add epoll() into the mix - now we can test multiple sockets.
 */
TEST_F(TestTcpTransport, Poll) {
  TcpListenSocket listen_socket = CreateListenSocket();
  uint16_t port = listen_socket.GetPort();
  ASSERT_NE(port, 0);
  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);
  std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

  const std::string hello = "Hello, World!";

  auto shared_message = std::make_shared<basis::core::transport::RawMessage>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());
  Epoll poller;

  core::transport::IncompleteRawMessage incomplete;
  auto callback = [&incomplete, &receiver, &poller, &hello](int fd, std::unique_lock<std::mutex>) {
    const std::string channel_name = "test";
    spdlog::info("Running poller callback on fd {}", fd);

    switch (receiver->ReceiveMessage(incomplete)) {

    case TcpReceiver::ReceiveStatus::DONE: {
      spdlog::debug("TcpReceiver Got full message");
      auto msg = incomplete.GetCompletedMessage();
      ASSERT_NE(msg, nullptr);
      spdlog::info("{}", spdlog::to_hex(msg->GetPacket()));
      spdlog::info("{}", (const char *)msg->GetPayload().data());
      ASSERT_STREQ((const char *)msg->GetPayload().data(), hello.c_str());

      // TODO: peek
      break;
    }
    case TcpReceiver::ReceiveStatus::DOWNLOADING: {
      break;
    }
    case TcpReceiver::ReceiveStatus::ERROR: {
      spdlog::error("{} bytes {} - got error {} {}", fd, incomplete.GetCurrentProgress(), errno, strerror(errno));
    }
    case TcpReceiver::ReceiveStatus::DISCONNECTED: {
      spdlog::error("Disconnecting from channel {}", channel_name);
      return;
    }
    }
    poller.ReactivateHandle(receiver->GetSocket().GetFd());
  };

  ASSERT_TRUE(poller.AddFd(receiver->GetSocket().GetFd(), callback));

  std::span<const std::byte> packet = shared_message->GetPacket();
  for (size_t i = 0; i < packet.size(); i++) {
    // todo: learn how to iterate on spans
    spdlog::trace("Sending byte {}: {}", i, *(packet.data() + i));
    Send(*sender, packet.data() + i, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  sender->SendMessage(shared_message);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // todo: test error conditions
}

/**
 * Add thread pool into the mix.
 */
TEST_F(TestTcpTransport, ThreadPool) {
  TcpListenSocket listen_socket = CreateListenSocket();
  uint16_t port = listen_socket.GetPort();
  ASSERT_NE(port, 0);
  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);
  std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

  const std::string hello = "Hello, World!";

  auto shared_message = std::make_shared<basis::core::transport::RawMessage>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());

  Epoll poller;
  core::threading::ThreadPool thread_pool(4);
  const std::string channel_name = "test";
  core::transport::IncompleteRawMessage incomplete;
  auto callback = [&](int fd, std::unique_lock<std::mutex> lock) {
    thread_pool.enqueue([&, lock = std::move(lock)] {
      spdlog::info("Running poller callback");
      switch (receiver->ReceiveMessage(incomplete)) {

      case TcpReceiver::ReceiveStatus::DONE: {
        spdlog::debug("TcpReceiver Got full message");
        auto msg = incomplete.GetCompletedMessage();
        std::string expected_msg = "Hello, World! ";
        expected_msg += channel_name;
        ASSERT_STREQ((char *)msg->GetPayload().data(), expected_msg.c_str());
        // TODO: peek
        break;
      }
      case TcpReceiver::ReceiveStatus::DOWNLOADING: {
        break;
      }
      case TcpReceiver::ReceiveStatus::ERROR: {
        spdlog::error("{}, {}: bytes {} - got error {} {}", fd, (void *)&incomplete, incomplete.GetCurrentProgress(),
                      errno, strerror(errno));
      }
      case TcpReceiver::ReceiveStatus::DISCONNECTED: {
        spdlog::error("Disconnecting from channel {}", channel_name);
        return;
      }
      }
      poller.ReactivateHandle(receiver->GetSocket().GetFd());
    });
  };

  ASSERT_TRUE(poller.AddFd(receiver->GetSocket().GetFd(), callback));
}

/**
 * Finally, add a MPSC queue.
 * @todo: someday we will want multiple consuming threads on the same queue.
 */
TEST_F(TestTcpTransport, MPSCQueue) {
  TcpListenSocket listen_socket = CreateListenSocket();
  uint16_t port = listen_socket.GetPort();
  ASSERT_NE(port, 0);
  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);
  std::unique_ptr<TcpSender> sender = AcceptOneKnownClient(listen_socket);

  const std::string hello = "Hello, World!";

  auto shared_message = std::make_shared<basis::core::transport::RawMessage>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());

  Epoll poller;
  core::threading::ThreadPool thread_pool(4);

  SimpleMPSCQueue<std::shared_ptr<core::transport::RawMessage>> output_queue;

  /**
   * Create callback, storing in the bind
   *
   * This allows the epoll interface to be completely unaware of what type of work it's being given.
   */
  std::string channel_name = "test";
  auto callback = [&](int fd, std::shared_ptr<core::transport::IncompleteRawMessage> incomplete) {
    /**
     * This is called by epoll when new data is available on a socket. We immediately do nothing with it, and instead
     * push the work off to the thread pool. This should be a very fast operation.
     */
    thread_pool.enqueue([&] {
      // It's an error to actually call this with multiple threads.
      // TODO: add debug only checks for this
      spdlog::debug("Running poller callback on {}", fd);

      switch (receiver->ReceiveMessage(*incomplete)) {

      case TcpReceiver::ReceiveStatus::DONE: {
        spdlog::debug("Got full message");
        output_queue.Emplace(incomplete->GetCompletedMessage());
        break;
      }
      case TcpReceiver::ReceiveStatus::DOWNLOADING: {
        spdlog::debug("Downloading...");
        break;
      }
      case TcpReceiver::ReceiveStatus::ERROR: {
        spdlog::error("{}, {}: bytes {} - got error {} {}", fd, (void *)incomplete.get(),
                      incomplete->GetCurrentProgress(), errno, strerror(errno));
      }
      case TcpReceiver::ReceiveStatus::DISCONNECTED: {
        spdlog::error("Disconnecting from channel {}", channel_name);
        return;
      }
      };
      poller.ReactivateHandle(fd);
      spdlog::debug("Rearmed");
    });
  };

  ASSERT_TRUE(poller.AddFd(
      receiver->GetSocket().GetFd(),
      std::bind(callback, std::placeholders::_1, std::make_shared<core::transport::IncompleteRawMessage>())));
  ASSERT_EQ(output_queue.Pop(0), std::nullopt);
  spdlog::info("Testing with one message");
  sender->SendMessage(shared_message);

  ASSERT_NE(output_queue.Pop(10), std::nullopt);
  ASSERT_EQ(output_queue.Pop(0), std::nullopt);
  ASSERT_EQ(output_queue.Size(), 0);
  spdlog::info("Got one message");

  spdlog::info("Testing with 10 messages");
  for (int i = 0; i < 10; i++) {
    sender->SendMessage(shared_message);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 0; i < 10; i++) {
    ASSERT_NE(output_queue.Pop(1), std::nullopt) << "Failed on message " << i;
  }
  ASSERT_EQ(output_queue.Pop(0), std::nullopt);
}
TEST_F(TestTcpTransport, Torture) {

  auto poller = std::make_unique<Epoll>();
  core::threading::ThreadPool thread_pool(4);

  SimpleMPSCQueue<std::pair<std::string, std::shared_ptr<core::transport::RawMessage>>> output_queue;

  /**
   * Create callback, storing in the bind
   *
   * This allows the epoll interface to be completely unaware of what type of work it's being given.
   *
   * The metadata here (channel_name) is completely arbitrary. Real code should make up a Context object to hold it all
   * in.
   */
  auto callback = [&thread_pool, poller = poller.get(), &output_queue](
                      int fd, std::unique_lock<std::mutex> lock, std::string channel_name, TcpReceiver *receiver,
                      std::shared_ptr<core::transport::IncompleteRawMessage> incomplete) {
    spdlog::info("Queuing work for {}", fd);

    /**
     * This is called by epoll when new data is available on a socket. We immediately do nothing with it, and instead
     * push the work off to the thread pool. This should be a very fast operation.
     */
    thread_pool.enqueue([fd, receiver, channel_name, &poller, &output_queue, incomplete, lock = std::move(lock)] {
      // It's an error to actually call this with multiple threads.
      // TODO: add debug only checks for this
      spdlog::debug("Running thread pool callback on {}", fd);
      switch (receiver->ReceiveMessage(*incomplete)) {

      case TcpReceiver::ReceiveStatus::DONE: {
        spdlog::debug("TcpReceiver Got full message");
        auto msg = incomplete->GetCompletedMessage();
        std::string expected_msg = "Hello, World! ";
        expected_msg += channel_name;
        ASSERT_STREQ((char *)msg->GetPayload().data(), expected_msg.c_str());
        std::pair<std::string, std::shared_ptr<core::transport::RawMessage>> out(channel_name, std::move(msg));
        output_queue.Emplace(std::move(out));

        // TODO: peek
        break;
      }
      case TcpReceiver::ReceiveStatus::DOWNLOADING: {
        break;
      }
      case TcpReceiver::ReceiveStatus::ERROR: {
        spdlog::error("{}, {}: bytes {} - got error {} {}", fd, (void *)incomplete.get(),
                      incomplete->GetCurrentProgress(), errno, strerror(errno));
      }
      case TcpReceiver::ReceiveStatus::DISCONNECTED: {
        spdlog::error("Disconnecting from channel {}", channel_name);
        return;
      }
      }
      poller->ReactivateHandle(fd);
      spdlog::info("Rearmed");
    });
  };

  spdlog::info("Stressing");
  constexpr int SENDER_COUNT = 10;
  constexpr int RECEIVERS_PER_SENDER = 10;
  constexpr int MESSAGES_PER_SENDER = 100;

  spdlog::info("Creating {} listen sockets", SENDER_COUNT);
  std::vector<TcpListenSocket> listen_sockets;
  for (int i = 0; i < SENDER_COUNT; i++) {
    listen_sockets.emplace_back(CreateListenSocket());
  }

  spdlog::set_level(spdlog::level::warn);

  // TODO: the naming here is missed up
  std::vector<std::unique_ptr<TcpReceiver>> receivers;
  std::vector<std::vector<std::unique_ptr<TcpSender>>> senders_by_index;
  std::vector<std::shared_ptr<basis::core::transport::RawMessage>> messages_to_send;

  spdlog::info("Creating {} connections for each listen socket", RECEIVERS_PER_SENDER);
  for (int sender_listen_socket_index = 0; sender_listen_socket_index < SENDER_COUNT; sender_listen_socket_index++) {
    const std::string hello = "Hello, World! " + std::to_string(sender_listen_socket_index);
    auto shared_message = std::make_shared<basis::core::transport::RawMessage>(
        basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
    strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());
    messages_to_send.emplace_back(std::move(shared_message));

    std::vector<std::unique_ptr<TcpSender>> senders;
    const uint16_t port = listen_sockets[sender_listen_socket_index].GetPort();
    spdlog::info("Listener {} port {}", sender_listen_socket_index, port);
    for (int receiver_index = 0; receiver_index < RECEIVERS_PER_SENDER; receiver_index++) {
      auto receiver = SubscribeToPort(port);
      ASSERT_NE(receiver, nullptr);
      ASSERT_TRUE(poller->AddFd(receiver->GetSocket().GetFd(),
                                std::bind(callback, std::placeholders::_1, std::placeholders::_2,
                                          std::to_string(sender_listen_socket_index), receiver.get(),
                                          std::make_shared<core::transport::IncompleteRawMessage>())));

      receivers.push_back(std::move(receiver));
      senders.push_back(AcceptOneKnownClient(listen_sockets[sender_listen_socket_index]));
    }

    senders_by_index.emplace_back(std::move(senders));
  }

  ASSERT_EQ(receivers.size(), RECEIVERS_PER_SENDER * SENDER_COUNT);
  ASSERT_EQ(senders_by_index.size(), SENDER_COUNT);

  spdlog::warn("Sending {} messages on each sender", MESSAGES_PER_SENDER);
  for (int message_count = 0; message_count < MESSAGES_PER_SENDER; message_count++) {
    for (int sender_index = 0; sender_index < SENDER_COUNT; sender_index++) {
      for (auto &sender : senders_by_index[sender_index]) {
        sender->SendMessage(messages_to_send[sender_index]);
      }
    }
  }
  spdlog::warn("Done sending, waiting now");

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  spdlog::warn("Done waiting, queue size is {}", output_queue.Size());

  ASSERT_EQ(output_queue.Size(), MESSAGES_PER_SENDER * RECEIVERS_PER_SENDER * SENDER_COUNT);

  std::unordered_map<std::string, size_t> counts;

  for (int i = 0; i < MESSAGES_PER_SENDER * RECEIVERS_PER_SENDER * SENDER_COUNT; i++) {
    auto msg = output_queue.Pop(0);
    ASSERT_NE(msg, std::nullopt);
    std::string hello = "Hello, World! " + msg->first;
    ASSERT_STREQ(hello.c_str(), (char *)msg->second->GetPayload().data());
    counts[hello]++;
  }

  for (auto p : counts) {
    ASSERT_EQ(p.second, MESSAGES_PER_SENDER * SENDER_COUNT);
  }

  // Spam a bunch more messages to catch errors on shutdown
  // TODO: actually add test case for catching these errors, rather than eyeballing the logs
  for (int message_count = 0; message_count < MESSAGES_PER_SENDER; message_count++) {
    for (int sender_index = 0; sender_index < SENDER_COUNT; sender_index++) {
      for (auto &sender : senders_by_index[sender_index]) {
        sender->SendMessage(messages_to_send[sender_index]);
      }
    }
  }
  // spdlog::set_level(spdlog::level::debug);
  spdlog::warn("Removing fds");
  // On my system this completes in 2-10ms if there are many many messages in flight - usually much less

  // Enable this to test disconnect behavior
  // senders_by_index.clear();
  for (auto &r : receivers) {
    poller->RemoveFd(r->GetSocket().GetFd());
  }
  spdlog::warn("Done removing fds");
  spdlog::warn("queue size is {}", output_queue.Size());
  spdlog::warn("Exiting");
}

} // namespace basis::plugins::transport