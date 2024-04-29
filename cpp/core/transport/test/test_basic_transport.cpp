#include <gtest/gtest.h>
#include <basis/core/transport/inproc.h>

TEST(Inproc, PubSub) {
    // Create a Coordinator
    InprocCoordinator coordinator;
    // Create a publisher
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
