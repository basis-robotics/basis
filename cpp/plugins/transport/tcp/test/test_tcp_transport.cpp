#include <memory>
#include <span>
#include <thread>

#include "basis/core/transport/transport.h"
#include "spdlog/cfg/env.h"
#include <basis/core/transport/transport_manager.h>
#include <basis/plugins/transport/epoll.h>
#include <basis/plugins/transport/tcp.h>
#include <gtest/gtest.h>

#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <queue>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <test.pb.h>
#pragma clang diagnostic pop

#include <basis/plugins/serialization/protobuf.h>

#include <google/protobuf/util/message_differencer.h>

using namespace basis::core::threading;

using namespace basis::core::networking;
using namespace basis::core::transport;

using namespace basis::plugins::transport;

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

  TcpSubscriber *GetTcpSubscriber(basis::core::transport::SubscriberBase *subscriber) {
    return dynamic_cast<TcpSubscriber *>(subscriber->transport_subscribers[0].get());
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
  auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
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
  const MessageHeader *header = msg->GetMessageHeader();
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
  // todo: saw this hung once
  // ...should be fixed now
  auto maybe_publisher = TcpPublisher::Create();
  ASSERT_TRUE(maybe_publisher.has_value());
  auto publisher = std::move(*maybe_publisher);
  uint16_t port = publisher->GetPort();
  ASSERT_NE(port, 0);
  auto publish_over_port = TcpPublisher::Create(port);
  spdlog::debug("Successfully created publisher on port {}", port);
  ASSERT_FALSE(publish_over_port.has_value());
  int error = publish_over_port.error().second;
  spdlog::debug("Failed to create another publisher on same port - got [{}: {}]", error, strerror(error));

  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);

  ASSERT_EQ(publisher->CheckForNewSubscriptions(), 1);
}

/**
 * Test creating a transport
 */
TEST_F(TestTcpTransport, TestTransport) {
  TcpTransport transport;
  std::shared_ptr<TransportPublisher> publisher = transport.Advertise("test", {"raw", "int", "", ""});
  ASSERT_NE(publisher, nullptr);
}

struct TestStruct {
  uint32_t foo = 3;
  float bar = 8.5;
  char baz[4] = "baz";

  friend auto operator<=>(const TestStruct &, const TestStruct &) = default;
};

/**
 * Test full pipeline with transport manager
 */
TEST_F(TestTcpTransport, TestWithManager) {

  TransportManager transport_manager;
  transport_manager.RegisterTransport("net_tcp", std::make_unique<TcpTransport>());

  auto send_msg = std::make_shared<TestProtoStruct>();
  send_msg->set_foo(3);
  send_msg->set_bar(8.5);
  send_msg->set_baz("baz");

  std::atomic<int> callback_times{0};
  SubscriberCallback<TestProtoStruct> callback = [&](std::shared_ptr<const TestProtoStruct> t) {
    spdlog::warn("Got TestProtoStruct {{ {} {} {} }}", t->foo(), t->bar(), t->baz());
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(*send_msg, *t));
    callback_times++;
  };

  std::atomic<int> raw_callback_times{0};
  TypeErasedSubscriberCallback raw_callback = [&]([[maybe_unused]] std::shared_ptr<MessagePacket> packet) {
    //TestStruct *t = (TestStruct *)packet->GetPayload().data();
    //spdlog::warn("Got a raw TestStruct {{ {} {} {} }}", t->foo, t->bar, t->baz);
    //ASSERT_EQ(*t, *send_msg);
    raw_callback_times++;
  };

  auto test_publisher =
      transport_manager.Advertise<TestProtoStruct>("test_struct");
  ASSERT_NE(test_publisher, nullptr);

  uint16_t port = 0;
  const std::string &info = test_publisher->GetPublisherInfo().transport_info[TCP_TRANSPORT_NAME];
  spdlog::info("publisher at {}", info);
  port = stoi(info);

  ASSERT_NE(port, 0);

  std::unique_ptr<TcpReceiver> receiver = SubscribeToPort(port);

  transport_manager.Update();

  ASSERT_EQ(test_publisher->GetTransportSubscriberCount(), 1);
  test_publisher->Publish(send_msg);
  // auto string_publisher = transport_manager.Advertise("test_string");
  // ASSERT_NE(publisher, nullptr);

  auto recv_msg = receiver->ReceiveMessage(1.0);
  // Ensure we have a message
  ASSERT_NE(recv_msg, nullptr);

  // TODO: reenable these
  // Ensure we didn't accidentally invoke the inproc transport
  //ASSERT_NE((TestStruct *)recv_msg->GetPayload().data(), send_msg.get());

  // Ensure we got what we sent
  //ASSERT_EQ(recv_msg->GetPayload().size(), sizeof(TestProtoStruct));
  //ASSERT_EQ(memcmp(recv_msg->GetPayload().data(), send_msg.get(), sizeof(TestProtoStruct)), 0);
  //ASSERT_EQ(memcmp(recv_msg->GetPayload().data(), send_msg.get(), sizeof(TestProtoStruct)), 0);

  auto overall_queue = std::make_shared<basis::core::containers::SubscriberOverallQueue>();
  auto output_queue = std::make_shared<basis::core::containers::SubscriberQueue>(overall_queue, 0);

  transport_manager.Update();

  basis::core::threading::ThreadPool work_thread_pool(4);

  std::shared_ptr<Subscriber<TestProtoStruct>> queue_subscriber =
      transport_manager.Subscribe<TestProtoStruct>(
          "test_struct", callback, &work_thread_pool, output_queue);
  std::shared_ptr<Subscriber<TestProtoStruct>> immediate_subscriber =
      transport_manager.Subscribe<TestProtoStruct>("test_struct", callback,
                                                                                         &work_thread_pool);

  std::shared_ptr<SubscriberBase> immediate_raw_subscriber =
      transport_manager.SubscribeRaw("test_struct", raw_callback, &work_thread_pool, nullptr, {});

  auto &pub_info = transport_manager.GetLastPublisherInfo();
  transport_manager.Update();

  ASSERT_EQ(test_publisher->GetTransportSubscriberCount(), 4);

  std::array<std::shared_ptr<SubscriberBase>, 3> subscribers = {queue_subscriber, immediate_subscriber,
                                                                immediate_raw_subscriber};

  ASSERT_EQ(queue_subscriber->GetPublisherCount(), 1);
  ASSERT_EQ(immediate_subscriber->GetPublisherCount(), 1);

  transport_manager.Update();
  ASSERT_EQ(test_publisher->GetTransportSubscriberCount(), 4);

  // todo: why do we have to manually connect this? the transport manager should be doing this
  for (auto &subscriber : subscribers) {
    // Handling identical publishers is a noop
    subscriber->HandlePublisherInfo(pub_info);
    subscriber->HandlePublisherInfo(pub_info);
    subscriber->HandlePublisherInfo(pub_info);
    ASSERT_EQ(subscriber->GetPublisherCount(), 1);
  }

  transport_manager.Update();
  ASSERT_EQ(test_publisher->GetTransportSubscriberCount(), 4);

  test_publisher->Publish(send_msg);

  // todo: handy dandy condition variable wrapper to not have to wait here
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Check immediate subscriber
  ASSERT_EQ(callback_times, 1);

  // Check queue subscriber
  auto event = overall_queue->Pop(basis::core::Duration::FromSecondsNanoseconds(10, 0));
  ASSERT_NE(event, std::nullopt);
  (*event)();
  ASSERT_EQ(callback_times, 2);

  // Check raw subscriber
  ASSERT_EQ(raw_callback_times, 1);
}

TEST_F(TestTcpTransport, TestWithProtobuf) {
  basis::core::threading::ThreadPool work_thread_pool(4);

  TransportManager transport_manager;
  transport_manager.RegisterTransport("net_tcp", std::make_unique<TcpTransport>());

  auto test_publisher = transport_manager.Advertise<TestProtoStruct>("test_proto");
  ASSERT_NE(test_publisher, nullptr);

  uint16_t port = 0;
  const std::string &info = test_publisher->GetPublisherInfo().transport_info[TCP_TRANSPORT_NAME];
  spdlog::info("publisher at {}", info);
  port = stoi(info);
  ASSERT_NE(port, 0);

  auto send_msg = std::make_shared<TestProtoStruct>();

  send_msg->set_foo(3);
  send_msg->set_bar(8.5);
  send_msg->set_baz("baz");

  std::atomic<int> callback_times{0};
  SubscriberCallback<TestProtoStruct> callback = [&](std::shared_ptr<const TestProtoStruct> msg) {
    spdlog::info("Got the message:\n{}", msg->DebugString());
    ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(*send_msg, *msg));

    callback_times++;
  };

  auto subscriber = transport_manager.Subscribe<TestProtoStruct>("test_proto", callback, &work_thread_pool);
#if 1
  transport_manager.Update();
  subscriber->HandlePublisherInfo(transport_manager.GetLastPublisherInfo());
#else
  TcpSubscriber *tcp_subscriber = GetTcpSubscriber(subscriber.get());
  ASSERT_NE(tcp_subscriber, nullptr);
  tcp_subscriber->ConnectToPort("127.0.0.1", port);
#endif
  transport_manager.Update();

  ASSERT_EQ(test_publisher->GetTransportSubscriberCount(), 1);

  test_publisher->Publish(send_msg);

  // TODO: handy dandy condition variable wrapper to not have to wait here
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(callback_times, 1);

  // TODO: send garbage data, ensure that we correctly don't get a callback
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

  auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());
  Epoll poller;

  IncompleteMessagePacket incomplete;
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

  auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());

  Epoll poller;
  ThreadPool thread_pool(4);
  const std::string channel_name = "test";
  IncompleteMessagePacket incomplete;
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

  auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
      basis::core::transport::MessageHeader::DataType::MESSAGE, hello.size() + 1);
  strcpy((char *)shared_message->GetMutablePayload().data(), hello.data());

  Epoll poller;
  ThreadPool thread_pool(4);

  basis::core::containers::SimpleMPSCQueue<std::shared_ptr<MessagePacket>> output_queue;

  /**
   * Create callback, storing in the bind
   *
   * This allows the epoll interface to be completely unaware of what type of work it's being given.
   */
  std::string channel_name = "test";
  auto callback = [&](int fd, std::shared_ptr<IncompleteMessagePacket> incomplete) {
    /**
     * This is called by epoll when new data is available on a socket. We immediately do nothing with it, and instead
     * push the work off to the thread pool. This should be a very fast operation.
     */
    thread_pool.enqueue([&, fd, incomplete = std::move(incomplete)] {
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

  ASSERT_NE(receiver->GetSocket().GetFd(), -1);

  ASSERT_TRUE(poller.AddFd(receiver->GetSocket().GetFd(),
                           std::bind(callback, std::placeholders::_1, std::make_shared<IncompleteMessagePacket>())));
  ASSERT_EQ(output_queue.Pop(basis::core::Duration::FromSecondsNanoseconds(0, 0)), std::nullopt);
  spdlog::info("Testing with one message");
  sender->SendMessage(shared_message);

  ASSERT_NE(output_queue.Pop(basis::core::Duration::FromSecondsNanoseconds(10, 0)), std::nullopt);
  ASSERT_EQ(output_queue.Pop(basis::core::Duration::FromSecondsNanoseconds(0, 0)), std::nullopt);
  ASSERT_EQ(output_queue.Size(), 0);
  spdlog::info("Got one message");

  spdlog::info("Testing with 10 messages");
  for (int i = 0; i < 10; i++) {
    sender->SendMessage(shared_message);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  for (int i = 0; i < 10; i++) {
    ASSERT_NE(output_queue.Pop(basis::core::Duration::FromSecondsNanoseconds(1, 0)), std::nullopt)
        << "Failed on message " << i;
  }
  ASSERT_EQ(output_queue.Pop(basis::core::Duration::FromSecondsNanoseconds(0, 0)), std::nullopt);
}

TEST_F(TestTcpTransport, Torture) {
  auto poller = std::make_unique<Epoll>();
  ThreadPool thread_pool(4);

  auto overall_queue = std::make_shared<basis::core::containers::SubscriberOverallQueue>();
  basis::core::containers::SubscriberQueue output_queue(overall_queue, 0);

  /**
   * Create callback, storing in the bind
   *
   * This allows the epoll interface to be completely unaware of what type of work it's being given.
   *
   * The metadata here (channel_name) is completely arbitrary. Real code should make up a Context object to hold it all
   * in.
   */
  auto callback = [&thread_pool, poller = poller.get(),
                   &output_queue](int fd, std::unique_lock<std::mutex> lock, std::string channel_name,
                                  TcpReceiver *receiver, std::shared_ptr<IncompleteMessagePacket> incomplete) {
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
        // OutputQueueEvent event = {
        //     .topic_name = channel_name, .packet = std::move(msg), .callback = TypeErasedSubscriberCallback()};
        //  todo
        output_queue.AddCallback([]() {});

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
  std::vector<std::shared_ptr<basis::core::transport::MessagePacket>> messages_to_send;

  spdlog::info("Creating {} connections for each listen socket", RECEIVERS_PER_SENDER);
  for (int sender_listen_socket_index = 0; sender_listen_socket_index < SENDER_COUNT; sender_listen_socket_index++) {
    const std::string hello = "Hello, World! " + std::to_string(sender_listen_socket_index);
    auto shared_message = std::make_shared<basis::core::transport::MessagePacket>(
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
                                          std::make_shared<IncompleteMessagePacket>())));

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
  spdlog::warn("Done waiting, queue size is {}", overall_queue->Size());

  ASSERT_EQ(overall_queue->Size(), MESSAGES_PER_SENDER * RECEIVERS_PER_SENDER * SENDER_COUNT);

  std::unordered_map<std::string, size_t> counts;

  for (int i = 0; i < MESSAGES_PER_SENDER * RECEIVERS_PER_SENDER * SENDER_COUNT; i++) {
    auto event = overall_queue->Pop();
    ASSERT_NE(event, std::nullopt);
    // std::string hello = "Hello, World! " + event->topic_name;
    // ASSERT_STREQ(hello.c_str(), (char *)event->packet->GetPayload().data());
    // TODO?
    (*event)();
    // counts[hello]++;
  }

  for (auto p : counts) {
    ASSERT_EQ(p.second, MESSAGES_PER_SENDER * SENDER_COUNT);
  }
  spdlog::warn("Processed {} sends", MESSAGES_PER_SENDER * RECEIVERS_PER_SENDER * SENDER_COUNT);

// Spam a bunch more messages to catch errors on shutdown
// TODO: actually add test case for catching these errors, rather than eyeballing the logs
#if 0
  for (int message_count = 0; message_count < MESSAGES_PER_SENDER; message_count++) {
    for (int sender_index = 0; sender_index < SENDER_COUNT; sender_index++) {
      for (auto &sender : senders_by_index[sender_index]) {
        sender->SendMessage(messages_to_send[sender_index]);
      }
    }
  }
#endif
  // spdlog::set_level(spdlog::level::debug);
  spdlog::warn("Removing fds");
  // On my system this completes in 2-10ms if there are many many messages in flight - usually much less

  // Enable this to test disconnect behavior
  // senders_by_index.clear();
  for (auto &r : receivers) {
    poller->RemoveFd(r->GetSocket().GetFd());
  }
  spdlog::warn("Done removing fds");
  spdlog::warn("queue size is {}", overall_queue->Size());
  spdlog::warn("Exiting");
}

TEST(TestIntegration, TcpAndInproc) {
  basis::core::threading::ThreadPool work_thread_pool(4);

  TransportManager transport_manager(std::make_unique<InprocTransport>());
  transport_manager.RegisterTransport("net_tcp", std::make_unique<TcpTransport>());

  auto publisher =
      transport_manager.Advertise<TestStruct, basis::core::serialization::RawSerializer>("test_tcp_inproc");
  transport_manager.Update();

  auto send_msg = std::make_shared<TestStruct>();

  std::atomic<int> num_recv = 0;
  auto subscriber = transport_manager.Subscribe<TestStruct, basis::core::serialization::RawSerializer>(
      "test_tcp_inproc",
      [&num_recv, &send_msg](std::shared_ptr<const TestStruct> recv_msg) {
        // Ensure this came over the shared transport
        ASSERT_EQ(send_msg.get(), recv_msg.get());
        num_recv++;
      },
      &work_thread_pool);
  transport_manager.Update();

  publisher->Publish(send_msg);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // Ensure that we don't get double subscribed - once for the inproc once for the tcp
  ASSERT_EQ(num_recv, 1);
}