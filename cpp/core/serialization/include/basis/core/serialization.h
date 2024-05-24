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
        template<typename T_MSG>
        static std::pair<std::unique_ptr<std::byte[]>, size_t> SerializeToBytes(const T_MSG& message ) {
            // TODO: figure out a good assertion here to avoid raw serializing unsafe types
            auto buffer = std::make_unique<std::byte[]>(sizeof(T_MSG));
            new (buffer.get()) T_MSG(message);
            return {std::move(buffer), sizeof(T_MSG)};
        }

        template<typename T_MSG>
        static std::unique_ptr<T_MSG> DeserializeFromBytes(std::span<const std::byte> bytes) {
            // Note: this may need a check for alignment - might need to use memcpy instead
            return std::make_unique<T_MSG>((T_MSG*)bytes);
        }
    };
}
template<typename T_MSG, typename Enable = void>
struct SerializationHandler {
    static_assert(false, "Handler for this type not found - please make sure you've included the proper serialization plugin");
};

// TODO: zero copy messages may want the ability to accept a shared_ptr and return back a shared_ptr instead
// signifying that serialization is just another view on the same data
template<typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type >
static std::pair<std::unique_ptr<std::byte[]>, size_t> SerializeToBytes(const T_MSG& message ) {
    return T_Serializer::SerializeToBytes(message);

}

// TODO: this forces a heap allocation of the message, there may be a desire for stack allocated messages
template<typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type >
static std::unique_ptr<T_MSG> DeserializeFromBytes(std::span<const std::byte> bytes) {
    return T_Serializer::template DeserializeFromBytes<T_MSG>(bytes);
}
}