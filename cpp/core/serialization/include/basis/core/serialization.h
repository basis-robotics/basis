#pragma once
/**
 * @file serialization.h
 * 
 * Contains the serialization interface used by the transport layer.
 */

#include <cassert>
#include <functional>
#include <span>

namespace basis {
namespace core::serialization {
/**
 * Base interface, used by all serializers. Will later contain ToJSON and other utilities.
 *
 * @todo ToJSON
 * @todo ToDebugString
 * @todo Capabilities per message type - such as ability to be transported over network at all
 */
class Serializer {
private:
  Serializer() = default;
public:
  virtual ~Serializer() = default;
private:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
  /**
   * Returns the size in bytes required for a message to be serialized to disk or as a network packet.
   */
  template <typename T_MSG> 
  static size_t GetSerializedSize(const T_MSG &message) {
    static_assert(false, "Please implement this template function");
  }

  /**
   * Writes `message` to the region of memory pointed to by `bytes`.
   *
   * @returns true on success, false on failure.
   */
  template <typename T_MSG> 
  static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> bytes) {
    static_assert(false, "Please implement this template function");
  }

  /**
   * Converts the region of memory pointed to by `bytes` into a message.
   *
   * @returns a complete message on success or nullptr on failure
   */
    template <typename T_MSG> static std::unique_ptr<T_MSG>
    DeserializeFromSpan(std::span<const std::byte> bytes) {
          static_assert(false, "Please implement this template function");
    }
#pragma clang diagnostic pop

};


/**
 * Serializer that simply uses the message passed in as a raw byte buffer.
 * Understandably, this won't work with any heap allocated structures.
 * It's recommended to not use this with 
 */
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

  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
    // TODO: this may need a check for alignment - might need to use memcpy instead
    return std::make_unique<T_MSG>(*reinterpret_cast<const T_MSG *>(bytes.data()));
  }
};
} // namespace core::serialization
template <typename T_MSG, typename Enable = void> struct SerializationHandler {
  static_assert(false,
                "Handler for this type not found - please make sure you've included the proper serialization plugin");
};

template <typename T_MSG> using SerializeGetSizeCallback = std::function<size_t(const T_MSG &)>;

template <typename T_MSG> using SerializeWriteSpanCallback = std::function<bool(const T_MSG &, std::span<std::byte>)>;

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

template <typename T_MSG> using DeserializeCallback = std::function<std::unique_ptr<T_MSG>(std::span<const std::byte>)>;

// TODO: this forces a heap allocation of the message, there may be a desire for stack allocated messages
template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
  return T_Serializer::template DeserializeFromSpan<T_MSG>(bytes);
}

} // namespace basis