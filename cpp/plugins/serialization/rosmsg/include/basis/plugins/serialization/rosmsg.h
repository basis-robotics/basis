#pragma once
/**
 * @file rosmsg.h
 * 
 * inspired by https://github.com/ros/roscpp_core/blob/noetic-devel/doc/roscpp.rst
 */


#include <basis/core/serialization.h>
#include <ros/serialization.h>
#include <ros/message_traits.h>

namespace basis {
namespace plugins::serialization {
/**
 * Main class, implementing the Serializer interface.
 */
class RosmsgSerializer : public core::serialization::Serializer {
public:

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
    ros::serialization::OStream out(reinterpret_cast<uint8_t*>(span.data()), span.size());
    ros::serialization::serialize(out, message);  

    // TODO: serialize doesn't provide an error output
    // this probably needs try/catch
    return true;
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  static size_t GetSerializedSize(const T_MSG &message) {
    return ros::serialization::serializationLength(message);
  }
  

  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
    auto parsed_message = std::make_unique<T_MSG>();

    // TODO: why does ros need a non-const pointer here
    ros::serialization::IStream in(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(bytes.data())), bytes.size());
    ros::serialization::deserialize(in,*parsed_message);

    return parsed_message;
  }
};
} // namespace plugins::serialization

/**
 * Helper to enable protobuf serializer by default for all `protobuf::Message`.
 */
 
template <typename T_MSG>
struct SerializationHandler<T_MSG, std::enable_if_t<ros::message_traits::IsMessage<T_MSG>::value>> {
  using type = plugins::serialization::RosmsgSerializer;
};

} // namespace basis