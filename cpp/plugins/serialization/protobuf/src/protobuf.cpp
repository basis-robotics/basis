#include <basis/plugins/serialization/protobuf.h>

DECLARE_AUTO_LOGGER_PLUGIN(serialization, protobuf)

namespace basis::plugins::serialization::protobuf {
google::protobuf::SimpleDescriptorDatabase ProtobufSerializer::protoDb;
google::protobuf::DescriptorPool ProtobufSerializer::protoPool{&ProtobufSerializer::protoDb};
google::protobuf::DynamicMessageFactory ProtobufSerializer::protoFactory(&ProtobufSerializer::protoPool);
std::unordered_set<std::string> ProtobufSerializer::known_schemas;

} // namespace basis::plugins::serialization::protobuf

extern "C" {

basis::core::serialization::SerializationPlugin *LoadPlugin() {
  return new basis::plugins::serialization::protobuf::ProtobufPlugin();
}
}