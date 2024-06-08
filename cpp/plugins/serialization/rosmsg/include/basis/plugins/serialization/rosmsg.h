#pragma once
/**
 * @file rosmsg.h
 *
 * inspired by https://github.com/ros/roscpp_core/blob/noetic-devel/doc/roscpp.rst
 */

#include <basis/core/serialization.h>
#include <ros/message_traits.h>
#include <ros/serialization.h>

#include <rosx_introspection/ros_parser.hpp>

namespace basis {
namespace plugins::serialization {
/**
 * Main class, implementing the Serializer interface.
 */
class RosmsgSerializer : public core::serialization::Serializer {
public:
  static constexpr char SERIALIZER_ID[] = "rosmsg";

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
    ros::serialization::OStream out(reinterpret_cast<uint8_t *>(span.data()), span.size());
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
    ros::serialization::IStream in(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(bytes.data())),
                                   bytes.size());
    ros::serialization::deserialize(in, *parsed_message);

    return parsed_message;
  }

  template <typename T_MSG> static basis::core::serialization::MessageSchema DumpSchema() {
    basis::core::serialization::MessageSchema schema;

    schema.serializer = SERIALIZER_ID;
    schema.name = ros::message_traits::DataType<T_MSG>::value();
    schema.schema = ros::message_traits::Definition<T_MSG>::value();
    schema.hash_id = ros::message_traits::MD5Sum<T_MSG>::value();

    return schema;
  }

  static std::optional<std::string> DumpMessageJSONString(std::span<const std::byte> span,
                                                          std::string_view schema_name) {
    std::string json;
    RosMsgParser::ROS_Deserializer deserializer;

    auto parser = parser_collection.getParser(std::string(schema_name));
    if (parser->deserializeIntoJson({(const uint8_t *)span.data(), span.size()}, &json, &deserializer, 2)) {
      return json;
    }
    return {};
  }

  static std::optional<std::string> DumpMessageString(std::span<const std::byte> span, std::string_view schema_name) {
    // rosx_introspection's interface is really not great, so use json for both cases
    return DumpMessageJSONString(span, schema_name);
  }

  static bool LoadSchema(std::string_view schema_name, std::string_view schema) {
    // Unsure why rosx_introspection keys by topic rather than schema
    parser_collection.registerParser(std::string(schema_name), std::string(schema_name), std::string(schema));
    return true; 
  }

  template <typename T_MSG>
  static basis::core::serialization::MessageTypeInfo DeduceMessageTypeInfo() {
    return {SERIALIZER_ID, ros::message_traits::DataType<T_MSG>::value()};
  };
protected:
  static RosMsgParser::ParsersCollection<RosMsgParser::ROS_Deserializer> parser_collection;
};

using RosMsgPlugin = core::serialization::AutoSerializationPlugin<RosmsgSerializer>;

} // namespace plugins::serialization

/**
 * Helper to enable rosmsg serializer by default for all `ros::Message`.
 */

template <typename T_MSG>
struct SerializationHandler<T_MSG, std::enable_if_t<ros::message_traits::IsMessage<T_MSG>::value>> {
  using type = plugins::serialization::RosmsgSerializer;
};

} // namespace basis