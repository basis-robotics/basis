
#include <gtest/gtest.h>

#include <basis/plugins/serialization/rosmsg.h>

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
  ros::serialization::OStream out(reinterpret_cast<uint8_t*>(serialize_buf.get()), size);
  ros::serialization::serialize(out, test_string);

  ros::serialization::IStream in(reinterpret_cast<uint8_t*>(serialize_buf.get()), size);

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

  std::unique_ptr<std_msgs::String> parsed_message =
       basis::DeserializeFromSpan<std_msgs::String>({bytes.get(), size});
  ASSERT_EQ(message, *parsed_message);
}