#pragma once

#include <memory>
#include <span>

namespace basis::core::transport {

struct MessageHeader {
    enum class DataType : uint8_t {
        INVALID = 0,
        HELLO,   // Initial connection packet, specifying data type, any transport specific options
        DISCONNECT, // Disconnect warning, with reason for disconnect
        SCHEMA,  // A schema, transport specific but human readable
        MESSAGE, // A message, transport specific
    /*
        MESSAGE_JSON = 3; // A message file, converted to human readable format
    */
        MAX_DATA_TYPE,
    };
    /*
    * Versioning is handled by the last byte of the magic.
    * Different header versions may have different sizes.
    * Version 0: Initial implementation
    */
    uint8_t magic_version[4] = {'B', 'A', 'S', 0};
    DataType data_type = DataType::INVALID;
    uint32_t data_size = 0;
    uint64_t send_time = 0xFFFFFFFF;

    uint8_t GetHeaderVersion() {
        return magic_version[3];
    } 
}  __attribute__((packed));


class RawMessage {
public:
    RawMessage(MessageHeader::DataType data_type, uint32_t data_size)
        : storage(std::make_unique<std::byte[]>(data_size + sizeof(MessageHeader)))
       // , span(storage.get(), data_size + sizeof(MessageHeader))
    {
        InitializeHeader(data_type, data_size);
    }

#if 0
    // More dangerous, assumes the constructor knows what they are doing
    // This will be needed for in place serialization
    // Actually, let's avoid this for now. We can ask protobuf the size up front, and then fill in
    RawMessage(std::unique_ptr<std::byte[]> storage, uint32_t total_size, uint32_t data_size) : storage(storage) {
        assert(data_size + sizeof(MessageHeader) >= total_size);
        InitializeHeader(data_size);
    }

    //...it may be useful to be able to ask a shared memory transport for allocation, and then pass in a non owning handle to it (but dangerous!)

#endif
    std::span<const std::byte> GetPacket() const {
        return std::span<const std::byte>(storage.get(), reinterpret_cast<const MessageHeader*>(storage.get())->data_size + sizeof(MessageHeader));
    }
    std::span<std::byte> GetMutablePayload() {
        return std::span<std::byte>(storage.get() + sizeof(MessageHeader), reinterpret_cast<MessageHeader*>(storage.get())->data_size );
    }
private:
    void InitializeHeader(MessageHeader::DataType data_type, const uint32_t data_size) {
        MessageHeader* header = reinterpret_cast<MessageHeader*>(storage.get());
        header->data_type = data_type;
        header->data_size = data_size;
    }

    std::unique_ptr<std::byte[]> storage;
};

class TransportSender {
    // todo: this needs error handling
    // TODO: do all transports actually need to declare this?
    virtual bool Send(const std::byte* data, size_t len) = 0;

    virtual void SendMessage(std::shared_ptr<RawMessage> message) = 0;
};

class TransportReceiver {
    virtual int Receive(char* buffer, size_t buffer_len, int timeout_s) = 0;
};

#if 0
class Transport {
    static virtual std::unique_ptr<TransportPublisher*> Advertise(std::string_view topic) = 0;
    static virtual std::unique_ptr<TransportPublisher*> Subscribe(std::string_view topic) = 0;
    
    virtual void Send(const char* data, size_t len) = 0;
    virtual int RecvInto(char* buffer, size_t buffer_len, int timeout_s) = 0;
}
#endif

} // namespace basis::core::transport