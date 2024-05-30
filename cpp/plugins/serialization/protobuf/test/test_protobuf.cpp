#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <test.pb.h>
#pragma clang diagnostic pop

#include <basis/plugins/serialization/protobuf.h>

#include <spdlog/spdlog.h>
/**
 * Test basic protobuf integration - just ensure that we've linked the library properly
 */
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

/**
 * Test the serializer interface itself.
 */
TEST(TestProto, TestSerializer) {
  static_assert(std::is_same_v<basis::SerializationHandler<TestExampleMessage>::type,
                               basis::plugins::serialization::ProtobufSerializer>);

  TestExampleMessage message;
  message.set_email("test@example.com");

  auto [bytes, size] = basis::SerializeToBytes(message);
  ASSERT_NE(bytes, nullptr);

  std::unique_ptr<TestExampleMessage> parsed_message =
      basis::DeserializeFromSpan<TestExampleMessage>({bytes.get(), size});
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(message, *parsed_message));
}

TEST(TestProto, Schema) {
  using namespace basis::plugins::serialization;
  // std::string schema = ProtobufSerializer::GetSchema<SchemaTestMessage>();

  basis::core::serialization::MessageSchema schema = ProtobufSerializer::DumpSchema<SchemaTestMessage>();

  spdlog::info("{}:\n{}", schema.name, schema.schema);

  SchemaTestMessage written_message;
  written_message.mutable_referenced()->set_data("referenced data");
  for (int i = 0; i < 10; i++) {
    written_message.add_repeated_u32(i);
  }
  written_message.set_an_enum(EnumTest::CORPUS_IMAGES);
  written_message.set_some_string("test string");

  auto [bytes, size] = basis::SerializeToBytes(written_message);
  ASSERT_NE(bytes, nullptr);

  ASSERT_TRUE(ProtobufSerializer::LoadSchema(schema.name, schema.schema));

  std::optional<std::string> read_message = ProtobufSerializer::DumpMessageString({bytes.get(), size}, schema.name);
  ASSERT_NE(read_message, std::nullopt);
  ASSERT_EQ(read_message, written_message.DebugString());

  std::optional<std::string> json_str =  ProtobufSerializer::DumpMessageJSON({bytes.get(), size}, schema.name);
  ASSERT_NE(json_str, std::nullopt);
  spdlog::info("Json str: {}", *json_str);
  
  SchemaTestMessage msg_from_json;
  ASSERT_TRUE(google::protobuf::util::JsonStringToMessage(*json_str, &msg_from_json, {}).ok());
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(written_message, msg_from_json));


}

/**
 * @todo test failure cases
 */