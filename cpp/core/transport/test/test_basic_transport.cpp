#include <gtest/gtest.h>
#include <basis/core/transport/inproc.h>
#include <thread>
using namespace basis::core::transport;

TEST(Inproc, PubSub) {
    // Create a Coordinator
    InprocCoordinator<int> coordinator;
    // Create a publisher
    auto publisher = coordinator.Advertise("topic");

    int num_recv = 0;

    auto subscriber = coordinator.Subscribe("topic", [&num_recv](const MessageEvent<int>& message) {
        GTEST_ASSERT_EQ(*message.message, num_recv++);
    });

    for(int i = 0; i < 10; i++) {
        publisher->Publish(i);
    }

    subscriber->ConsumeMessages();
    GTEST_ASSERT_EQ(num_recv, 10);
}

TEST(Inproc, PubSubWait) {
    // Create a Coordinator
    InprocCoordinator<int> coordinator;
    // Create a publisher
    auto publisher = coordinator.Advertise("topic");

    int num_recv = 0;

    auto subscriber = coordinator.Subscribe("topic", [&num_recv](const MessageEvent<int>& message) {
        GTEST_ASSERT_EQ(*message.message, num_recv++);
    });

    std::thread pub_thread([&](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for(int i = 0; i < 10; i++) {
            publisher->Publish(i);
        }
    });

    subscriber->ConsumeMessages(true);
    GTEST_ASSERT_GT(num_recv, 0);
    pub_thread.join();
    subscriber->ConsumeMessages();
    GTEST_ASSERT_EQ(num_recv, 10);
}
