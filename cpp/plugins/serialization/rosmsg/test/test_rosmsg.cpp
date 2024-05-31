
#include <gtest/gtest.h>

#include <basis/plugins/serialization/rosmsg.h>

#include <sensor_msgs/PointCloud2.h>
#include <std_msgs/String.h>

#include <spdlog/spdlog.h>

/**
 * Test basic protobuf integration - just ensure that we've linked the library properly
 */
TEST(TestRosMsg, Basic) {
  std_msgs::String test_string;
  test_string.data = "this is a test of ROS";

  std::stringstream ss;
  ss << test_string;

  const uint32_t size = ros::serialization::serializationLength(test_string);
  spdlog::info("Ros msg size {} contents:\n{}", size, ss.str());

  auto serialize_buf = std::make_unique<std::byte[]>(size);
  ros::serialization::OStream out(reinterpret_cast<uint8_t *>(serialize_buf.get()), size);
  ros::serialization::serialize(out, test_string);

  ros::serialization::IStream in(reinterpret_cast<uint8_t *>(serialize_buf.get()), size);

  std_msgs::String test_string_2;
  ros::serialization::deserialize(in, test_string_2);

  ASSERT_EQ(test_string, test_string_2);
}

/**
 * Test the serializer interface itself.
 */
TEST(TestRosMsg, TestSerializer) {
  static_assert(std::is_same_v<basis::SerializationHandler<std_msgs::String>::type,
                               basis::plugins::serialization::RosmsgSerializer>);
  std_msgs::String message;
  message.data = "this is a test of ROS";

  auto [bytes, size] = basis::SerializeToBytes(message);
  ASSERT_NE(bytes, nullptr);

  std::unique_ptr<std_msgs::String> parsed_message = basis::DeserializeFromSpan<std_msgs::String>({bytes.get(), size});
  ASSERT_EQ(message, *parsed_message);
}

TEST(TestRosMsg, TestSchema) {
  using namespace basis::plugins::serialization;
  RosMsgParser::ParsersCollection<RosMsgParser::ROS_Deserializer> parser_collection;
  RosMsgParser::ROS_Deserializer deserializer;
  {
    auto schema = RosmsgSerializer::DumpSchema<std_msgs::String>();
    ASSERT_TRUE(RosmsgSerializer::LoadSchema(schema.name, schema.schema));

    std_msgs::String message;
    message.data = "this is a test of ROS Deserialization";

    auto [bytes, size] = basis::SerializeToBytes(message);
    ASSERT_NE(bytes, nullptr);

    ASSERT_NE(RosmsgSerializer::DumpMessageString({bytes.get(), size}, schema.name), std::nullopt);
  }
  {
    auto schema = basis::plugins::serialization::RosmsgSerializer::DumpSchema<sensor_msgs::PointCloud2>();
    ASSERT_TRUE(RosmsgSerializer::LoadSchema(schema.name, schema.schema));

    sensor_msgs::PointCloud2 message;
    message.is_bigendian = false;
    message.height = 2;
    message.width = 16;
    message.data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    auto [bytes, size] = basis::SerializeToBytes(message);
    ASSERT_NE(bytes, nullptr);

    ASSERT_NE(RosmsgSerializer::DumpMessageString({bytes.get(), size}, schema.name), std::nullopt);
  }
}
