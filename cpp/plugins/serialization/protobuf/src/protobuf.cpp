#include <basis/plugins/serialization/protobuf.h>


namespace basis::plugins::serialization {
google::protobuf::SimpleDescriptorDatabase ProtobufSerializer::protoDb;
google::protobuf::DescriptorPool ProtobufSerializer::protoPool{&ProtobufSerializer::protoDb};
google::protobuf::DynamicMessageFactory ProtobufSerializer::protoFactory(&ProtobufSerializer::protoPool);
std::unordered_set<std::string> ProtobufSerializer::known_schemas;

}