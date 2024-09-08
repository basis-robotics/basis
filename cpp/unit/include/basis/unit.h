#pragma once
#include "basis/core/time.h"
#include "basis/synchronizers/synchronizer_base.h"
#include <basis/core/coordinator_connector.h>
#include <basis/core/logging.h>
#include <basis/core/threading/thread_pool.h>
#include <basis/core/transport/transport_manager.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>

namespace basis {
class DeterministicReplayer;
class UnitManager;

std::unique_ptr<basis::core::transport::TransportManager>
CreateStandardTransportManager(basis::RecorderInterface *recorder = nullptr);

void StandardUpdate(basis::core::transport::TransportManager *transport_manager,
                    basis::core::transport::CoordinatorConnector *coordinator_connector);

struct UnitInitializeOptions {
  bool create_subscribers = true;
};

struct HandlerPubSub {
  using TopicMap = std::map<std::string, std::shared_ptr<const void>>;
  using HandlerExecutingCallback = std::function<TopicMap()>;
  using TypeErasedCallback = std::function<void(const std::shared_ptr<const void>, HandlerExecutingCallback *)>;

  virtual void OnRateSubscriberTypeErased(basis::core::MonotonicTime now, HandlerExecutingCallback *callback) = 0;

  std::map<std::string, TypeErasedCallback> type_erased_callbacks;
  std::vector<std::string> outputs;
  std::optional<basis::core::Duration> rate_duration;
};

template <typename MessageType, bool IS_RAW> struct RawSerializationHelper {
  using type = basis::core::serialization::RawSerializer;
};

template <typename MessageType> struct RawSerializationHelper<MessageType, false> {
  using type = SerializationHandler<MessageType>::type;
};

template <typename T_DERIVED, bool HAS_RATE, size_t INPUT_COUNT>
struct HandlerPubSubWithOptions : public HandlerPubSub {
  constexpr static size_t input_count = INPUT_COUNT;

  template <int INDEX, typename ON_CONSUME>
  auto OnMessageHelper(auto *synchronizer, const auto msg, ON_CONSUME on_consume = nullptr) {
    typename T_DERIVED::Synchronizer::MessageSumType consume_msgs;
    typename T_DERIVED::Synchronizer::MessageSumType *consume_msgs_ptr = nullptr;
    if constexpr (!HAS_RATE) {
      consume_msgs_ptr = &consume_msgs;
    }
    const bool consumed = synchronizer->template OnMessage<INDEX>(msg, consume_msgs_ptr);
    if constexpr (!HAS_RATE) {
      if (consumed) {
        on_consume(basis::core::MonotonicTime::Now(), *consume_msgs_ptr);
      }
    }
  }

  template <int INDEX> auto CreateOnMessageCallback() {
    return [this](const auto msg) {
      T_DERIVED *derived = ((T_DERIVED *)this);
      return OnMessageHelper<INDEX>(
          derived->synchronizer.get(), msg,
          [derived](basis::core::MonotonicTime now, const T_DERIVED::Synchronizer::MessageSumType &msgs) {
            derived->RunHandlerAndPublish(now, msgs);
          });
    };
  }

  // Note: it'd be interesting to see if this works properly with vector types or not
  template <int INDEX> auto CreateTypeErasedOnMessageCallback() {
    return [this](const std::shared_ptr<const void> msg, HandlerExecutingCallback *callback) {
      T_DERIVED *derived = ((T_DERIVED *)this);
      using TupleElementType = std::tuple_element_t<INDEX, typename T_DERIVED::Synchronizer::MessageSumType>;
      using MessageType = typename basis::synchronizers::ExtractMessageType<TupleElementType>::Type;
      auto type_correct_msg = std::static_pointer_cast<MessageType>(msg);

      return OnMessageHelper<INDEX>(
          derived->synchronizer.get(), type_correct_msg,
          [callback, derived](basis::core::MonotonicTime now, const T_DERIVED::Synchronizer::MessageSumType &msgs) {
            *callback = [derived, now, msgs]() { return derived->RunHandlerAndPublish(now, msgs).ToTopicMap(); };
          });
    };
  }

  void OnRateSubscriber(const basis::core::MonotonicTime &now) {
    if constexpr (HAS_RATE) {
      T_DERIVED *derived = ((T_DERIVED *)this);

      auto msgs = derived->synchronizer->ConsumeIfReady();
      if (msgs) {
        derived->RunHandlerAndPublish(now, *msgs);
      }
    } else {
      static_assert(false, "OnRateSubscriber called on Handler with no rate specified");
    }
  }

  virtual void OnRateSubscriberTypeErased(basis::core::MonotonicTime now, HandlerExecutingCallback *callback) override {
    T_DERIVED *derived = ((T_DERIVED *)this);

    auto msgs = derived->synchronizer->ConsumeIfReady();
    if (msgs) {
      *callback = [derived, now, msgs]() { return derived->RunHandlerAndPublish(now, *msgs).ToTopicMap(); };
    }
  }

  template <int INDEX>
  void SetupInput(const basis::UnitInitializeOptions &options,
                  basis::core::transport::TransportManager *transport_manager,
                  std::shared_ptr<basis::core::transport::OutputQueue> output_queue,
                  basis::core::threading::ThreadPool *thread_pool) {
    T_DERIVED *derived = ((T_DERIVED *)this);

    if (options.create_subscribers) {
      using MessageType = std::remove_const_t<typename basis::synchronizers::ExtractMessageType<
          typename std::tuple_element_t<INDEX, typename T_DERIVED::Synchronizer::MessageSumType>>::Type>;
      auto subscriber_member_ptr = std::get<INDEX>(T_DERIVED::subscribers);

      constexpr bool is_raw = std::string_view(T_DERIVED::subscription_serializers[INDEX]) == "raw";

      derived->*subscriber_member_ptr =
          transport_manager->Subscribe<MessageType, typename RawSerializationHelper<MessageType, is_raw>::type>(
              derived->subscription_topics[INDEX], CreateOnMessageCallback<INDEX>(), thread_pool, output_queue);
    }

    type_erased_callbacks[derived->subscription_topics[INDEX]] = CreateTypeErasedOnMessageCallback<INDEX>();
  }

  void SetupInputs(const basis::UnitInitializeOptions &options,
                   basis::core::transport::TransportManager *transport_manager,
                   std::shared_ptr<basis::core::transport::OutputQueue> output_queue,
                   basis::core::threading::ThreadPool *thread_pool) {
    // Magic to iterate from 0...INPUT_COUNT, c++ really needs constexpr for
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (SetupInput<I>(options, transport_manager, output_queue, thread_pool), ...);
    }(std::make_index_sequence<INPUT_COUNT>());
  }
};

class Unit {
public:
  Unit(std::string_view unit_name)
      : unit_name(unit_name), logger(basis::core::logging::CreateLogger(std::string(unit_name))) {}

  virtual ~Unit() { spdlog::drop(std::string(unit_name)); }

  void WaitForCoordinatorConnection() {
    while (!coordinator_connector) {
      coordinator_connector = basis::core::transport::CoordinatorConnector::Create();
      if (!coordinator_connector) {
        BASIS_LOG_WARN("No connection to the coordinator, waiting 1 second and trying again");
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  }

  void CreateTransportManager(basis::RecorderInterface *recorder = nullptr) {
    // todo: it may be better to pass these in - do we want one transport manager per unit ?
    // probably yes, so that they each get an ID

    transport_manager = CreateStandardTransportManager(recorder);
  }

  const std::string &Name() const { return unit_name; }

  spdlog::logger &Logger() { return *logger; }

  // override this, should be called once by main()
  virtual void Initialize(const UnitInitializeOptions &options = {}) = 0;

  virtual void Update([[maybe_unused]] std::atomic<bool> *stop_token = nullptr,
                      [[maybe_unused]] const basis::core::Duration &max_sleep_duration =
                          basis::core::Duration::FromSecondsNanoseconds(0, 0)) {
    StandardUpdate(transport_manager.get(), coordinator_connector.get());
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  [[nodiscard]] std::shared_ptr<core::transport::Publisher<T_MSG>>
  Advertise(std::string_view topic,
            core::serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    return transport_manager->Advertise<T_MSG, T_Serializer>(topic, std::move(message_type));
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  [[nodiscard]] std::shared_ptr<core::transport::Subscriber<T_MSG>>
  Subscribe(std::string_view topic, core::transport::SubscriberCallback<T_MSG> callback,
            basis::core::threading::ThreadPool *work_thread_pool,
            std::shared_ptr<core::transport::OutputQueue> output_queue = nullptr,
            core::serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    return transport_manager->Subscribe<T_MSG, T_Serializer>(topic, std::move(callback), work_thread_pool, output_queue,
                                                             std::move(message_type));
  }

  using DeserializationHelper = std::function<std::shared_ptr<const void>(std::span<const std::byte>)>;
  const DeserializationHelper &GetDeserializationHelper(const std::string &type) {
    return deserialization_helpers.at(type);
  }

protected:
  std::string unit_name;
  std::shared_ptr<spdlog::logger> logger;
  const std::shared_ptr<spdlog::logger> &AUTO_LOGGER = logger;
  std::unique_ptr<basis::core::transport::TransportManager> transport_manager;
  std::unique_ptr<basis::core::transport::CoordinatorConnector> coordinator_connector;

  // Used when autogenerated only
  friend class basis::DeterministicReplayer;
  friend class basis::UnitManager;
  std::map<std::string, HandlerPubSub *> handlers;

  // Helpers to convert a byte buffer to a type erased message pointer
  std::unordered_map<std::string, DeserializationHelper> deserialization_helpers;
};

/**
 * A simple unit where all handlers are run mutally exclusive from eachother - uses a queue for all outputs, which adds
 * some amount of latency
 */
class SingleThreadedUnit : public Unit {
protected:
  using Unit::Update;

public:
  using Unit::Advertise;
  using Unit::Initialize;
  using Unit::Unit;

  virtual void Update(std::atomic<bool> *stop_token, const basis::core::Duration &max_sleep_duration) override {
    Unit::Update(stop_token, max_sleep_duration);
    // TODO: this won't necessarily sleep the max amount - this might be okay but could be confusing

    // try to get a single event, with a wait time
    if (auto event = output_queue->Pop(max_sleep_duration)) {
      (*event)();
    }

    // Try to drain the buffer of events
    while (auto event = output_queue->Pop()) {
      if ((stop_token != nullptr && *stop_token)) {
        break;
      }
      (*event)();
    }
    // todo: it's possible that we may want to periodically schedule Update() for the output queue
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  [[nodiscard]] std::shared_ptr<core::transport::Subscriber<T_MSG>>
  Subscribe(std::string_view topic, core::transport::SubscriberCallback<T_MSG> callback,
            core::serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    return Unit::Subscribe<T_MSG, T_Serializer>(topic, callback, &thread_pool, nullptr, std::move(message_type));
  }

  std::shared_ptr<basis::core::transport::OutputQueue> output_queue =
      std::make_shared<basis::core::transport::OutputQueue>();
  basis::core::threading::ThreadPool thread_pool{4};
};

// todo: interface for multithreaded units

} // namespace basis