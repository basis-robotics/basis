#include <gtest/gtest.h>
#include <basis/core/transport/inproc.h>

using namespace basis::core::transport;

TEST(Inproc, PubSub) {
    // Create a Coordinator
    InprocCoordinator<int> coordinator;
    // Create a publisher
    auto publisher = coordinator.Advertise("topic");

    auto subscriber = coordinator.Subscribe("topic", [](const MessageEvent<int>& message) {
        printf("Received message %i\n", *message.message);
    });

    for(int i = 0; i < 10; i++) {
        publisher->Publish(i);
    }

    subscriber->ConsumeMessages();
    // Create a subscriber
    // Subscribe the subscriber to the publisher
    // Publish a message
    // Check that the subscriber received the message

}

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}
