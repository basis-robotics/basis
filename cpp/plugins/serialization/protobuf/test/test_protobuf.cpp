#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "test.pb.h"

#include <basis/plugins/serialization/protobuf.h>

TEST(TestProto, Basic) {
  TestExampleMessage message;
  message.set_email("test@example.com");

  size_t size = message.ByteSizeLong();
  auto array = std::make_unique<std::byte[]>(size);
  message.SerializeToArray(array.get(), size);

  TestExampleMessage parsed_message;
  parsed_message.ParseFromArray(array.get(), size);

  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(message, parsed_message));
}

TEST(TestProto, TestSerializer) {
  static_assert(std::is_same_v<basis::SerializationHandler<TestExampleMessage>::type,
                               basis::plugins::serialization::ProtobufSerializer>);

  TestExampleMessage message;
  message.set_email("test@example.com");

  auto [bytes, size] = basis::SerializeToBytes(message);
  ASSERT_NE(bytes, nullptr);

  std::unique_ptr<TestExampleMessage> parsed_message = basis::DeserializeFromSpan<TestExampleMessage>({bytes.get(), size});
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(message, *parsed_message));
}