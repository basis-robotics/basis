#include <basis/core/coordinator.h>
#include <gtest/gtest.h>

TEST(TestCoordinator, BasicTest) {
  basis::core::transport::Coordinator coordinator = *basis::core::transport::Coordinator::Create();

  auto connector = basis::core::transport::CoordinatorConnector::Create();
  ASSERT_NE(connector, nullptr);

  basis::core::transport::PublisherInfo pub_info;
  pub_info.publisher_id = basis::core::transport::CreatePublisherId();
  pub_info.topic = "test_topic";
  pub_info.transport_info["net_tcp"] = "1234";

  basis::core::transport::proto::TransportManagerInfo sent_info;
  *sent_info.add_publishers() = pub_info.ToProto();

  connector->SendTransportManagerInfo(sent_info);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  coordinator.Update();
}

struct TestRawStruct {
  uint32_t foo = 3;
  float bar = 8.5;
  char baz[4] = "baz";
};

TEST(TestCoordinator, TestPubSubOrder) {
  using namespace basis::core::threading;

  using namespace basis::core::networking;
  using namespace basis::core::transport;

  using namespace basis::plugins::transport;
  basis::core::transport::Coordinator coordinator = *basis::core::transport::Coordinator::Create();

  auto connector = basis::core::transport::CoordinatorConnector::Create();

  auto thread_pool_manager = std::make_shared<ThreadPoolManager>();
  TransportManager transport_manager;
  transport_manager.RegisterTransport("net_tcp", std::make_unique<TcpTransport>(thread_pool_manager));

  std::atomic<int> callback_times{0};
  SubscriberCallback<TestRawStruct> callback = [&](std::shared_ptr<const TestRawStruct> t) {
    spdlog::warn("Got the message {} {} {}", t->foo, t->bar, t->baz);
    callback_times++;
  };

  connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
  coordinator.Update();
  connector->Update();

  std::shared_ptr<Subscriber<TestRawStruct>> prev_sub =
    transport_manager.Subscribe<TestRawStruct, basis::core::serialization::RawSerializer>("test_struct", callback);

  transport_manager.Update();
  connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
  coordinator.Update();
    connector->Update();

  auto test_publisher =
      transport_manager.Advertise<TestRawStruct, basis::core::serialization::RawSerializer>("test_struct");

  transport_manager.Update();
  connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
  coordinator.Update();
  std::shared_ptr<Subscriber<TestRawStruct>> after_sub =
      transport_manager.Subscribe<TestRawStruct, basis::core::serialization::RawSerializer>("test_struct", callback);

  transport_manager.Update();
  connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
  coordinator.Update();
  connector->Update();

  // ASSERT_EQ(test_publisher->GetSubscriberCount(), 2);
  auto send_msg = std::make_shared<const TestRawStruct>();
  test_publisher->Publish(send_msg);

  // ASSERT_EQ(test_publisher->GetSubscriberCount(), 1);
}