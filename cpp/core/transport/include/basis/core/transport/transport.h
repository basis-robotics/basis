#pragma once

#include <memory>
#include <span>

#include <spdlog/spdlog.h>

#include "inproc.h"
#include "publisher.h"
#include "publisher_info.h"
#include "subscriber.h"

#include "simple_mpsc.h"

#include <basis/core/serialization.h>
#include <basis/core/threading/thread_pool.h>

namespace basis::core::transport {

/**
 * Helper for holding incomplete messages.
 */
class IncompleteMessagePacket {
public:
  IncompleteMessagePacket() = default;

  std::span<std::byte> GetCurrentBuffer() {
    if (incomplete_message) {
      spdlog::trace("Pulling payload");
      std::span<std::byte> ret = incomplete_message->GetMutablePayload();
      return ret.subspan(progress_counter);
    } else {
      spdlog::trace("Pulling header");
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

// TODO: use MessageEvent
// TODO: don't store the packet directly, store a weak reference to the transport subscriber
// TODO: just capture everything in a closure
struct OutputQueueEvent {
  std::string topic_name;
  std::unique_ptr<MessagePacket> packet;
  TypeErasedSubscriberCallback callback;
};

//using OutputQueue = SimpleMPSCQueue<OutputQueueEvent>;
using OutputQueue = SimpleMPSCQueue<std::function<void()>>;

class Transport {
public:
  Transport() = default;
  virtual ~Transport() = default;
  virtual std::shared_ptr<TransportPublisher> Advertise(std::string_view topic,
                                                        serialization::MessageTypeInfo type_info) = 0;
  virtual std::shared_ptr<TransportSubscriber> Subscribe(std::string_view topic, TypeErasedSubscriberCallback callback,
                                                         basis::core::threading::ThreadPool *work_thread_pool,
                                                         serialization::MessageTypeInfo type_info) = 0;

  /**
   * Implementations (ie TransportManager) should call this function at a regular rate.
   * @todo: do we want to keep this or enforce each transport taking care of its own update calls?
   */
  virtual void Update() {}
};

} // namespace basis::core::transport