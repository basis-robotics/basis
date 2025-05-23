#pragma once
/**
 * @file protobuf.h
 *
 * A serialization plugin used to (de)serialize `protobuf::Message`
 * Depends on `libprotobuf-dev` (though in the short term, `-lite` may be compatible) and protobuf-compiler for
 * compiling any message definitions.
 *
 * @todo Useful for later for deserialization without having compile time awareness of a message:
 *   https://vdna.be/site/index.php/2016/05/google-protobuf-at-run-time-deserialization-example-in-c/
 *   https://mcap.dev/guides/cpp/protobuf
 */

#include <unordered_set>

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include <spdlog/spdlog.h>

#include <basis/core/serialization.h>
#include <basis/core/serialization/message_type_info.h>

#include <basis/core/logging/macros.h>

DEFINE_AUTO_LOGGER_PLUGIN(serialization, protobuf)

namespace basis {
namespace plugins::serialization::protobuf {

/**
 * Main class, implementing the Serializer interface.
 */
class ProtobufSerializer : public core::serialization::Serializer {
public:
  static constexpr char SERIALIZER_ID[] = "protobuf";

  template <typename T_MSG> static bool SerializeToSpan(const T_MSG &message, std::span<std::byte> span) {
    return message.SerializeToArray(span.data(), span.size());
  }

  template <typename T_MSG> static size_t GetSerializedSize(const T_MSG &message) { return message.ByteSizeLong(); }

  template <typename T_MSG> static std::unique_ptr<T_MSG> DeserializeFromSpan(std::span<const std::byte> bytes) {
    // TODO: https://protobuf.dev/reference/cpp/arenas/
    // this either requires shared_ptr return from this _or_ an explicit MessageWithArena type
    auto parsed_message = std::make_unique<T_MSG>();

    if (!parsed_message->ParseFromArray(bytes.data(), bytes.size())) {
      // note: this is pretty liberal in what it accepts
      BASIS_LOG_ERROR("Unable to parse a message");
      return nullptr;
    }

    if (!parsed_message->IsInitialized()) {
      return nullptr;
    }

    return parsed_message;
  }

  static std::unique_ptr<google::protobuf::Message> LoadMessageFromSchema(std::span<const std::byte> span,
                                                                          std::string_view schema_name) {
    auto descriptor = protoPool.FindMessageTypeByName(std::string(schema_name));

    if (!descriptor) {
      BASIS_LOG_ERROR("Haven't seen schema {} before", schema_name);
      return {};
    }

    auto read_message = std::unique_ptr<google::protobuf::Message>(protoFactory.GetPrototype(descriptor)->New());
    if (!read_message->ParseFromArray(span.data(), span.size())) {
      BASIS_LOG_ERROR("failed to parse message using included schema");
      return {};
    }
    return read_message;
  }

  static std::optional<std::string> DumpMessageString(std::span<const std::byte> span, std::string_view schema_name) {
    // TODO: we could use TextFormat here, but this might be easier to read
    std::unique_ptr<google::protobuf::Message> message = LoadMessageFromSchema(span, schema_name);
    if (message) {
      return message->DebugString();
    }
    return {};
  }

  static std::optional<std::string> DumpMessageJSONString(std::span<const std::byte> span,
                                                          std::string_view schema_name) {
    // TODO: we could likely use TypeResolver and BinaryToJsonStream here
    // but neither DebugString nor TextFormat have implementations
    std::unique_ptr<google::protobuf::Message> message = LoadMessageFromSchema(span, schema_name);
    if (message) {
      std::string out;
      google::protobuf::util::MessageToJsonString(*message, &out, {});
      return out;
    }
    return {};
  }

  template <typename T_MSG> static basis::core::serialization::MessageSchema DumpSchema() {
    const google::protobuf::Descriptor *descriptor = T_MSG::descriptor();
    basis::core::serialization::MessageSchema schema;
    schema.serializer = SERIALIZER_ID;
    schema.name = descriptor->full_name();
    auto msg = fdSet(descriptor);
    // schema.schema = msg.SerializeAsString();
    schema.schema_efficient = msg.SerializeAsString();
    google::protobuf::TextFormat::PrintToString(msg, &schema.schema);
    // schema.human_readable = msg.DebugString();
    return schema;
  }

  // todo: does this need a mutex?
  static bool LoadSchema(std::string_view schema_name, std::string_view schema) {
    const std::string schema_name_str(schema_name);
    if (known_schemas.contains(schema_name_str)) {
      return true;
    }
    known_schemas.insert(schema_name_str);

    google::protobuf::io::ArrayInputStream stream(schema.data(), schema.size());
    google::protobuf::FileDescriptorSet fdSet;
    if (!google::protobuf::TextFormat::Parse(&stream, &fdSet)) {
      return false;
    }

    google::protobuf::FileDescriptorProto unused;
    for (int i = 0; i < fdSet.file_size(); ++i) {
      const auto &file = fdSet.file(i);
      if (!protoDb.FindFileByName(file.name(), &unused)) {
        if (!protoDb.Add(file)) {
          BASIS_LOG_ERROR("failed to add definition {} to protoDB", file.name());
        }
      }
    }

    return true;
  }

  template <typename T_MSG> static basis::core::serialization::MessageTypeInfo DeduceMessageTypeInfo() {
    return {SERIALIZER_ID, T_MSG::descriptor()->full_name(), GetMCAPMessageEncoding(), GetMCAPSchemaEncoding()};
  };

  static const char *GetMCAPSchemaEncoding() {
    // https://mcap.dev/spec/registry#protobuf
    return "protobuf";
  }

  static const char *GetMCAPMessageEncoding() {
    // https://mcap.dev/spec/registry#protobuf
    return "protobuf";
  }

protected:
  // https://mcap.dev/guides/cpp/protobuf
  // Recursively adds all `fd` dependencies to `fd_set`.
  static void fdSetInternal(google::protobuf::FileDescriptorSet &fd_set, std::unordered_set<std::string> &files,
                            const google::protobuf::FileDescriptor *fd) {
    for (int i = 0; i < fd->dependency_count(); ++i) {
      const auto *dep = fd->dependency(i);
      auto [_, inserted] = files.insert(dep->name());
      if (!inserted)
        continue;
      fdSetInternal(fd_set, files, fd->dependency(i));
    }
    fd->CopyTo(fd_set.add_file());
  }

  // https://mcap.dev/guides/cpp/protobuf
  // Returns a serialized google::protobuf::FileDescriptorSet containing
  // the necessary google::protobuf::FileDescriptor's to describe d.
  static google::protobuf::FileDescriptorSet fdSet(const google::protobuf::Descriptor *d) {
    std::string res;
    std::unordered_set<std::string> files;
    google::protobuf::FileDescriptorSet fd_set;
    fdSetInternal(fd_set, files, d->file());
    return fd_set;
  }

  // https://mcap.dev/guides/cpp/protobuf
  // todo: thread safety
  static google::protobuf::DescriptorPool protoPool;
  static google::protobuf::SimpleDescriptorDatabase protoDb;
  static google::protobuf::DynamicMessageFactory protoFactory;
  static std::unordered_set<std::string> known_schemas;
};

using ProtobufPlugin = core::serialization::AutoSerializationPlugin<ProtobufSerializer>;

} // namespace plugins::serialization::protobuf

/**
 * Helper to enable protobuf serializer by default for all `protobuf::Message`.
 */
template <typename T_MSG>
struct SerializationHandler<T_MSG, std::enable_if_t<std::is_base_of_v<google::protobuf::Message, T_MSG>>> {
  using type = plugins::serialization::protobuf::ProtobufSerializer;
};

} // namespace basis
