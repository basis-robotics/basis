#pragma once
/**
 * @file serialization.h
 *
 * Contains the serialization interface used by the transport layer.
 */

#include <cassert>
#include <functional>
#include <memory>
#include <span>

#include "serialization/message_type_info.h"

namespace basis {
namespace core::serialization {

struct MessageSchema {
  std::string serializer;
  std::string name;
  std::string schema;
  std::string
      schema_efficient; // Optional - some serializers may have a more efficient representation wanted by the recorder
  std::string hash_id;
};

/**
 * Base interface, used by all serializers. Will later contain ToJSON and other utilities.
 *
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
  template <typename T_MSG> static size_t GetSerializedSize(const T_MSG &message) {
    static_assert(false, "Please implement this template function");
  }

  /**
   * Writes `message` to the region of memory pointed to by `bytes`.
   *
   * @returns true on success, false on failure.
   */
  template <typename T_MSG> static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> bytes) {
    static_assert(false, "Please implement this template function");
  }

  /**
   * Converts the region of memory pointed to by `bytes` into a message.
   *
   * @returns a complete message on success or nullptr on failure
   *
   * @todo this forces a heap allocation of the message, there may be a desire for stack allocated messages
   */
  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
    static_assert(false, "Please implement this template function");
  }
#pragma clang diagnostic pop
};

class SerializationPlugin {
public:
  static constexpr char PLUGIN_TYPE[] = "serialization";

  virtual ~SerializationPlugin() = default;

  virtual std::string_view GetPluginName() = 0;

  virtual bool LoadSchema(std::string_view schema_name, std::string_view schema) = 0;

  virtual std::optional<std::string> DumpMessageString(std::span<const std::byte> span,
                                                       std::string_view schema_name) = 0;

  virtual std::optional<std::string> DumpMessageJSONString(std::span<const std::byte> span,
                                                           std::string_view schema_name) = 0;
};

template <typename T_Serializer> class AutoSerializationPlugin : public SerializationPlugin {
public:
  AutoSerializationPlugin() = default;

  std::string_view GetPluginName() override { return T_Serializer::SERIALIZER_ID; }

  virtual bool LoadSchema(std::string_view schema_name, std::string_view schema) override {
    return T_Serializer::LoadSchema(schema_name, schema);
  }

  virtual std::optional<std::string> DumpMessageString(std::span<const std::byte> span,
                                                       std::string_view schema_name) override {
    return T_Serializer::DumpMessageString(span, schema_name);
  }

  virtual std::optional<std::string> DumpMessageJSONString(std::span<const std::byte> span,
                                                           std::string_view schema_name) override {
    return T_Serializer::DumpMessageJSONString(span, schema_name);
  }
};

/**
 * Serializer that simply uses the message passed in as a raw byte buffer.
 * Understandably, this won't work with any heap allocated structures, nor with anything to a pointer to other memory.
 *
 * @todo ensure structure is pod
 */
class RawSerializer : public Serializer {
public:
  template <typename T_MSG> static serialization::MessageTypeInfo DeduceMessageTypeInfo() {
    // todo: deprecate
    return {"raw", "unknown", "", ""};
  }

  template <typename T_MSG> static size_t GetSerializedSize(const T_MSG &message) { return sizeof(message); }

  template <typename T_MSG> static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
    if (span.size() < sizeof(message)) {
      return false;
    }

    // Should not use placement new here, due to alignment
    memcpy(span.data(), &message, sizeof(message));

    return true;
  }

  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
    // TODO: this may need a check for alignment - might need to use memcpy instead
    return std::make_unique<T_MSG>(*reinterpret_cast<const T_MSG *>(bytes.data()));
  }
};

} // namespace core::serialization

/**
 * Helper to query the Serializer used for a message.
 * Will never assume RawSerializer.
 *
 * @example
 *   typename T_Serializer = SerializationHandler<T_MSG>::type;
 *   std::shared_ptr<const T_MSG> message = T_Serializer::template DeserializeFromSpan<T_MSG>(packet->GetPayload());
 */
template <typename T_MSG, typename Enable = void> struct SerializationHandler {
  using type = core::serialization::Serializer;

  static_assert(false, "Serialization handler for this type not found - please make sure you've included the proper "
                       "serialization plugin");
};

/**
 * Helpers to standardize callbacks used by the transport layer for serialization.
 */
template <typename T_MSG> using SerializeGetSizeCallback = std::function<size_t(const T_MSG &)>;
template <typename T_MSG> using SerializeWriteSpanCallback = std::function<bool(const T_MSG &, std::span<std::byte>)>;

/**
 * Helper to serialize a message to bytes.
 *
 * @returns {message_pointer, size} on success
 * @returns {nullptr, 0} on failure
 */
template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static std::pair<std::unique_ptr<std::byte[]>, size_t> SerializeToBytes(const T_MSG &message) {
  const size_t size = T_Serializer::GetSerializedSize(message);
  auto buffer = std::make_unique<std::byte[]>(size);
  if (T_Serializer::SerializeToSpan(message, {buffer.get(), size})) {
    return {std::move(buffer), size};
  }
  return {nullptr, 0};
}

/**
 * Helper to standardize callback used by the transport layer for deserialization.
 */
template <typename T_MSG> using DeserializeCallback = std::function<std::unique_ptr<T_MSG>(std::span<const std::byte>)>;

/**
 * Helper to deserialize a message from a region of memory.
 */
template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
  return T_Serializer::template DeserializeFromSpan<T_MSG>(bytes);
}

} // namespace basis