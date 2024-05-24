#pragma once
#include <google/protobuf/message.h>

#include <basis/core/serialization.h>


// Useful for later 
// https://vdna.be/site/index.php/2016/05/google-protobuf-at-run-time-deserialization-example-in-c/

namespace basis {
namespace plugins::serialization {
class ProtobufSerializer : public core::serialization::Serializer {
public:
  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
    return message.SerializeToArray(span.data(), span.size());
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  static size_t GetSerializedSize(const T_MSG &message) {
    return message.ByteSizeLong();
  }

  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
    // TODO: https://protobuf.dev/reference/cpp/arenas/
    // this either requires shared_ptr return from this _or_ an explicit MessageWithArena type
    auto parsed_message = std::make_unique<T_MSG>();

    if (!parsed_message->ParseFromArray(bytes.data(), bytes.size())) {
      return nullptr;
    }

    return parsed_message;
  }
};
} // namespace plugins::serialization
template <typename T_MSG>
struct SerializationHandler<T_MSG, std::enable_if_t<std::is_base_of_v<google::protobuf::Message, T_MSG>>> {
  using type = plugins::serialization::ProtobufSerializer;
};

} // namespace basis