#pragma once
#include <google/protobuf/message.h>

#include <basis/core/serialization.h>

namespace basis {
    namespace plugins::serialization {
        class ProtobufSerializer : public core::serialization::Serializer {
        public:
            template<typename T_MSG>
            static std::pair<std::unique_ptr<std::byte[]>, size_t> SerializeToBytes(const T_MSG& message ) {

                size_t size = message.ByteSizeLong();
                auto array = std::make_unique<std::byte[]>(size);

                if(!message.SerializeToArray(array.get(), size)) {
                    return {nullptr, 0};
                }

                return {std::move(array), size};

            }
            // TODO: this forces a heap allocation of the message, there may be a desire for stack allocated messages
            template<typename T_MSG>
            static std::unique_ptr<T_MSG> DeserializeFromBytes(std::span<const std::byte> bytes) {

                auto parsed_message = std::make_unique<T_MSG>();
                if(!parsed_message->ParseFromArray(bytes.data(), bytes.size())) {
                    return nullptr;
                }

                return parsed_message;

            }
        };
    }
    template<typename T_MSG>
    struct SerializationHandler<T_MSG, std::enable_if_t<std::is_base_of_v<google::protobuf::Message, T_MSG>>> {
        using type = plugins::serialization::ProtobufSerializer;
    };

}