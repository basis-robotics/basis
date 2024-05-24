#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "test.pb.h"

#include <basis/plugins/serialization/protobuf.h>

TEST(TestProto, Basic) {
  TestMessage message;
  message.set_email("test@example.com");

  size_t size = message.ByteSizeLong();
  auto array = std::make_unique<std::byte[]>(size);
  message.SerializeToArray(array.get(), size);

  TestMessage parsed_message;
  parsed_message.ParseFromArray(array.get(), size);

  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(message, parsed_message));
}

TEST(TestProto, TestSerializer) {
  static_assert(std::is_same_v<basis::SerializationHandler<TestMessage>::type,
                               basis::plugins::serialization::ProtobufSerializer>);

  TestMessage message;
  message.set_email("test@example.com");

  auto [bytes, size] = basis::SerializeToBytes(message);
  ASSERT_NE(bytes, nullptr);

  std::unique_ptr<TestMessage> parsed_message = basis::DeserializeFromSpan<TestMessage>({bytes.get(), size});
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(message, *parsed_message));
}