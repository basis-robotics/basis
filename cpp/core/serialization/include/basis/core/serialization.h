#pragma once

#include <span>

// useful https://vdna.be/site/index.php/2016/05/google-protobuf-at-run-time-deserialization-example-in-c/
namespace basis {
namespace core::serialization {
class Serializer {

public:
  virtual ~Serializer() = default;
};

class RawSerializer : public Serializer {
public:
  template <typename T_MSG> static size_t GetSerializedSize(const T_MSG &message) { return sizeof(message); }

  template <typename T_MSG> static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
    if (span.size() < sizeof(message)) {
      return false;
    }
    // new (span.data()) T_MSG(message);
    memcpy(span.data(), &message, sizeof(message));

    return true;
  }

  template <typename T_MSG>
  static std::pair<std::unique_ptr<std::byte[]>, size_t> SerializeToBytes(const T_MSG &message) {
    // TODO: figure out a good assertion here to avoid raw serializing unsafe types
    auto buffer = std::make_unique<std::byte[]>(sizeof(T_MSG));
    new (buffer.get()) T_MSG(message);
    return {std::move(buffer), sizeof(T_MSG)};
  }

  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromBytes(std::span<const std::byte> bytes) {
    // Note: this may need a check for alignment - might need to use memcpy instead
    return std::make_unique<T_MSG>((T_MSG *)bytes);
  }
};
} // namespace core::serialization
template <typename T_MSG, typename Enable = void> struct SerializationHandler {
  static_assert(false,
                "Handler for this type not found - please make sure you've included the proper serialization plugin");
};

// DELETEME

template <typename T_MSG> using SerializeGetSizeCallback = std::function<size_t(const T_MSG &)>;

template <typename T_MSG> using SerializeWriteSpanCallback = std::function<bool(const T_MSG &, std::span<std::byte> &)>;

template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static size_t GetSerializedSize(const T_MSG &message) {
  return T_Serializer::GetSerializedSize(message);
}

template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
  return T_Serializer::SerializeToSpan(message, span);
}

template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static std::pair<std::unique_ptr<std::byte[]>, size_t> SerializeToBytes(const T_MSG &message) {
  const size_t size = T_Serializer::GetSerializedSize(message);
  auto buffer = std::make_unique<std::byte[]>(size);
  if (T_Serializer::SerializeToSpan(message, {buffer.get(), size})) {
    return {std::move(buffer), size};
  }
  return {nullptr, 0};
}

// TODO: this forces a heap allocation of the message, there may be a desire for stack allocated messages
template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static std::unique_ptr<T_MSG> DeserializeFromBytes(std::span<const std::byte> bytes) {
  return T_Serializer::template DeserializeFromBytes<T_MSG>(bytes);
}

} // namespace basis