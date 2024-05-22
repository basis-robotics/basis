#pragma once

#include <memory>
#include <span>

#include "inproc.h"
#include "message_type_info.h"
#include "publisher.h"
#include "subscriber.h"
#include "thread_pool_manager.h"

#include "simple_mpsc.h"

namespace basis::core::transport {

/**
 * Helper for holding incomplete messages.
 *
 * TODO: use instructions are out of date
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
class IncompleteMessagePacket {
public:
  IncompleteMessagePacket() = default;

  std::span<std::byte> GetCurrentBuffer() {
    if (incomplete_message) {
      std::span<std::byte> ret = incomplete_message->GetMutablePayload();
      return ret.subspan(progress_counter);
    } else {
      return std::span<std::byte>(incomplete_header + progress_counter, sizeof(MessageHeader) - progress_counter);
    }
  }

  bool AdvanceCounter(size_t amount) {
    progress_counter += amount;
    if (!incomplete_message && progress_counter == sizeof(MessageHeader)) {
      // todo: check for header validity here
      progress_counter = 0;
      incomplete_message = std::make_unique<MessagePacket>(completed_header);
    }
    if (!incomplete_message) {
      return false;
    }
    return progress_counter == incomplete_message->GetMessageHeader()->data_size;
  }

  std::unique_ptr<MessagePacket> GetCompletedMessage() {
    // assert(incomplete_message);
    // assert(progress_counter == incomplete_message->GetMessageHeader()->data_size);
    progress_counter = 0;
    return std::move(incomplete_message);
  }

  size_t GetCurrentProgress() { return progress_counter; }

private:
  union {
    std::byte incomplete_header[sizeof(MessageHeader)] = {};
    MessageHeader completed_header;
  };
  std::unique_ptr<MessagePacket> incomplete_message;

  size_t progress_counter = 0;
};

/**
 * Helper class
 * @todo move to another file, it's implementation detail.
 */
class TransportSender {
public:
  virtual ~TransportSender() = default;

private:
  // todo: this needs error handling
  // TODO: do all transports actually need to declare this?
  virtual bool Send(const std::byte *data, size_t len) = 0;

  // TODO: why is this a shared_ptr?
  // ah, is it because this needs to be shared across multiple senders?
  virtual void SendMessage(std::shared_ptr<MessagePacket> message) = 0;
};

class TransportReceiver {
public:
  virtual ~TransportReceiver() = default;

private:
  virtual bool Receive(std::byte *buffer, size_t buffer_len, int timeout_s) = 0;
};

using OutputQueue = SimpleMPSCQueue<std::pair<std::string, TypeErasedSubscriberCallback>>;

class Transport {
public:
  Transport(std::shared_ptr<basis::core::transport::ThreadPoolManager> thread_pool_manager)
      : thread_pool_manager(thread_pool_manager) {}
  virtual ~Transport() = default;
  virtual std::shared_ptr<TransportPublisher> Advertise(std::string_view topic, MessageTypeInfo type_info) = 0;
  virtual std::shared_ptr<TransportSubscriber> Subscribe(std::string_view topic, TypeErasedSubscriberCallback callback, OutputQueue* output_queue,  MessageTypeInfo type_info) = 0;

  /**
   * Implementations should call this function at a regular rate.
   * @todo: do we want to keep this or enforce each transport taking care of its own update calls?
   */
  virtual void Update() {}

protected:
  /// Thread pools are shared across transports
  std::shared_ptr<basis::core::transport::ThreadPoolManager> thread_pool_manager;
};

class TransportManager {
public:
  TransportManager(std::unique_ptr<InprocTransport> inproc = nullptr) : inproc(std::move(inproc)) {}
  // todo: deducing a raw type should be an error unless requested
  template <typename T>
  std::shared_ptr<Publisher<T>> Advertise(std::string_view topic,
                                          MessageTypeInfo message_type = DeduceMessageTypeInfo<T>()) {
    std::shared_ptr<InprocPublisher<T>> inproc_publisher;
    if (inproc) {
      inproc_publisher = inproc->Advertise<T>(topic);
    }
    std::vector<std::shared_ptr<TransportPublisher>> tps;
    for (auto &[transport_name, transport] : transports) {
      tps.push_back(transport->Advertise(topic, message_type));
    }
    auto publisher = std::make_shared<Publisher<T>>(topic, message_type, std::move(tps), inproc_publisher);
    publishers.emplace(std::string(topic), publisher);
    return publisher;
  }

  template <typename T_MSG>
  std::shared_ptr<Subscriber<T_MSG>> Subscribe(std::string_view topic, SubscriberCallback<T_MSG> callback, core::transport::OutputQueue* output_queue = nullptr,
                                           MessageTypeInfo message_type = DeduceMessageTypeInfo<T_MSG>()) {
    std::shared_ptr<InprocSubscriber<T_MSG>> inproc_subscriber;

    [[maybe_unused]] TypeErasedSubscriberCallback outer_callback = [callback](std::shared_ptr<MessagePacket> packet) {
      // todo: deserialize
      // todo: for raw sends we can just move the data rather than copying

      std::shared_ptr<const T_MSG> message{new T_MSG(*(T_MSG*)packet->GetPayload().data())};
      callback(std::move(message));
    };

    if (inproc) {
    #if 0
      inproc_subscriber = inproc->Subscribe<T>(topic, [](MessageEvent<T_MSG>){});
        // TODO
    #endif
    }

    std::vector<std::shared_ptr<TransportSubscriber>> tps;

    for (auto &[transport_name, transport] : transports) {
      tps.push_back(transport->Subscribe(topic, outer_callback, output_queue, message_type));
    }

    auto subscriber = std::make_shared<Subscriber<T_MSG>>(topic, message_type, std::move(tps), inproc_subscriber);
    subscribers.push_back(subscriber);
    return subscriber;
  }

  /**
   *
   * @todo error handling, fail if there's already one of the same name
   * @todo can we ask the transport name from the transport?
   */
  void RegisterTransport(std::string_view transport_name, std::unique_ptr<Transport> transport) {
    transports.emplace(std::string(transport_name), std::move(transport));
  }

  void Update() {
    for (auto &[_, transport] : transports) {
      transport->Update();
    }
  }

protected:
  std::unique_ptr<InprocTransport> inproc;

  std::unordered_map<std::string, std::unique_ptr<Transport>> transports;

  std::unordered_multimap<std::string, std::weak_ptr<PublisherBase>> publishers;

  // TODO: these need to be by topic name, also
  std::vector<std::weak_ptr<SubscriberBase>> subscribers;
};

} // namespace basis::core::transport