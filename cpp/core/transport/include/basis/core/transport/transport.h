#pragma once

#include <memory>
#include <span>

namespace basis::core::transport {

struct MessageHeader {
    enum DataType : uint8_t {
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
    uint8_t reserved[3] = {};
    uint32_t data_size = 0;
    uint64_t send_time = 0xFFFFFFFF;
    

    uint8_t GetHeaderVersion() {
        return magic_version[3];
    } 
}  __attribute__((packed));

// We use placement new to initialize these, ensure there's no destructors
static_assert(std::is_standard_layout<MessageHeader>::value);
static_assert(std::is_trivially_destructible<MessageHeader>::value);

// todo: rename to PackagedMessage
class RawMessage {
public:
    /**
     * Construct given a packet type and size. Typically used when preparing to send data.
     */
    RawMessage(MessageHeader::DataType data_type, uint32_t data_size)
        : storage(std::make_unique<std::byte[]>(data_size + sizeof(MessageHeader)))
    {
        InitializeHeader(data_type, data_size);
    }

    /**
     * Construct given a header. Typically used when receiving data.
     */
    RawMessage(MessageHeader header)
        : storage(std::make_unique<std::byte[]>(header.data_size + sizeof(MessageHeader)))
    {
        *(MessageHeader*)storage.get() = header;
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

    const MessageHeader* GetMessageHeader() const {
        return reinterpret_cast<const MessageHeader*>(storage.get());
    }

    std::span<const std::byte> GetPacket() const {
        return std::span<const std::byte>(storage.get(), GetMessageHeader()->data_size + sizeof(MessageHeader));
    }

    std::span<const std::byte> GetPayload() const {
        return std::span<const std::byte>(storage.get() + sizeof(MessageHeader), GetMessageHeader()->data_size );
    }

    std::span<std::byte> GetMutablePayload() {
        return std::span<std::byte>(storage.get() + sizeof(MessageHeader), GetMessageHeader()->data_size );
    }
private:
    MessageHeader* GetMutableMessageHeader() {
        return reinterpret_cast<MessageHeader*>(storage.get());
    }

    void InitializeHeader(MessageHeader::DataType data_type, const uint32_t data_size) {
        MessageHeader* header = new(storage.get()) MessageHeader;

        header->data_type = data_type;
        header->data_size = data_size;
    }

    std::unique_ptr<std::byte[]> storage;
};

/**
 * Helper for holding incomplete messages.
 *
 * To use: 

    size_t count = 0;
    do {
        // Request space to download in
        std::span<std::byte> buffer = incomplete.GetCurrentBuffer();

        // Download some bytes
        count = recv(buffer.data(), buffer.size());
    // Continue downloading until we've gotten the whole message
    } while(!incomplete.AdvanceCounter(count));

 */
class IncompleteRawMessage {
public:
    IncompleteRawMessage() = default;

    std::span<std::byte> GetCurrentBuffer() {
        if(incomplete_message) {
            std::span<std::byte> ret = incomplete_message->GetMutablePayload();
            return ret.subspan(progress_counter);
        }
        else {
            return std::span<std::byte>(incomplete_header + progress_counter, sizeof(MessageHeader) - progress_counter);
        }
    }

    bool AdvanceCounter(size_t amount) {
        progress_counter += amount;
        if(!incomplete_message && progress_counter == sizeof(MessageHeader)) {
            // todo: check for header validity here
            progress_counter = 0;
            incomplete_message = std::make_unique<RawMessage>(completed_header);
        }
        if(!incomplete_message) {
            return false;
        }
        return progress_counter == incomplete_message->GetMessageHeader()->data_size;
    }

    std::unique_ptr<RawMessage> GetCompletedMessage() {
        //assert(incomplete_message);
        //assert(progress_counter == incomplete_message->GetMessageHeader()->data_size); 
        progress_counter = 0;
        return std::move(incomplete_message);
    }

    size_t GetCurrentProgress() {
        return progress_counter;
    }
private:
    union {
        std::byte incomplete_header[sizeof(MessageHeader)] = {};
        MessageHeader completed_header;
    };
    std::unique_ptr<RawMessage> incomplete_message;

    size_t progress_counter = 0;
};

class TransportSender {
    // todo: this needs error handling
    // TODO: do all transports actually need to declare this?
    virtual bool Send(const std::byte* data, size_t len) = 0;

    // TODO: why is this a shared_ptr?
    virtual void SendMessage(std::shared_ptr<RawMessage> message) = 0;
};

class TransportReceiver {
    virtual bool Receive(std::byte* buffer, size_t buffer_len, int timeout_s) = 0;
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