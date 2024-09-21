#pragma once
#include "basis/core/time.h"
#include "basis/core/transport/convertable_inproc.h"
#include "basis/synchronizers/synchronizer_base.h"
#include <basis/core/containers/subscriber_callback_queue.h>
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
  using TypeErasedCallback =
      std::function<void(const std::shared_ptr<const void>, HandlerExecutingCallback *, std::string_view)>;

  virtual void OnRateSubscriberTypeErased(basis::core::MonotonicTime now, HandlerExecutingCallback *callback) = 0;

  std::map<std::string, TypeErasedCallback> type_erased_callbacks;
  std::vector<std::string> outputs;
  std::optional<basis::core::Duration> rate_duration;
};

// Helper - if we're raw serialization, use it
template <typename MessageType, bool IS_RAW> struct RawSerializationHelper {
  using type = basis::core::serialization::RawSerializer;
};

template <typename MessageType> struct RawSerializationHelper<MessageType, false> {
  using type = SerializationHandler<MessageType>::type;
};

// Helper - go from either:
// std::shared_ptr<T_MSG> to <T_MSG, NoAdditionalInproc>
template <typename MessageType, bool IS_STD_VARIANT = basis::synchronizers::IsStdVariant<MessageType>::value>
struct VariantHelper {
  using type = std::remove_const_t<typename MessageType::element_type>;
  using inproc_type = basis::core::transport::NoAdditionalInproc;
};
// or std::variant<std::monostate, std::shared_ptr<T_MSG>, std::shared_ptr<T_ALTERNATE_TYPE>> to <T_MSG,
// T_ALTERNATE_TYPE>
template <typename MessageType> struct VariantHelper<MessageType, true> {
  using type = std::remove_const_t<typename std::variant_alternative_t<1, MessageType>::element_type>;
  using inproc_type = std::remove_const_t<typename std::variant_alternative_t<2, MessageType>::element_type>;
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

  template <int INDEX, bool IS_ALTERNATE_INPROC> auto CreateOnMessageCallback() {
    using MaybeVariantMessageType = std::remove_const_t<typename basis::synchronizers::ExtractFromContainer<
        typename std::tuple_element_t<INDEX, typename T_DERIVED::Synchronizer::MessageSumType>>::Type>;

    using MessageType =
        std::conditional_t<IS_ALTERNATE_INPROC, typename VariantHelper<MaybeVariantMessageType>::inproc_type,
                           typename VariantHelper<MaybeVariantMessageType>::type>;

    // If we don't have an alternate, don't even bother, construct a default
    if constexpr (std::is_same_v<MessageType, basis::core::transport::NoAdditionalInproc>) {
      return std::function<void(std::shared_ptr<const basis::core::transport::NoAdditionalInproc>)>();
    }
    // Otherwise, create a message of the proper type
    else {
      return std::function([this](std::shared_ptr<const MessageType> msg) {
        SPDLOG_INFO(IS_ALTERNATE_INPROC ? "inproc message" : "typed message");
        T_DERIVED *derived = ((T_DERIVED *)this);
        return OnMessageHelper<INDEX>(
            derived->synchronizer.get(), msg,
            [derived](basis::core::MonotonicTime now, const T_DERIVED::Synchronizer::MessageSumType &msgs) {
              derived->RunHandlerAndPublish(now, msgs);
            });
      });
    }
  }

  // Note: it'd be interesting to see if this works properly with vector types or not
  // TODO: fix variant types
  template <int INDEX> TypeErasedCallback CreateTypeErasedOnMessageCallback() {
    return [this](const std::shared_ptr<const void> msg, HandlerExecutingCallback *callback,
                  std::string_view type_name) {
      T_DERIVED *derived = ((T_DERIVED *)this);
      using MaybeVariantMessageType = std::remove_const_t<typename basis::synchronizers::ExtractFromContainer<
          typename std::tuple_element_t<INDEX, typename T_DERIVED::Synchronizer::MessageSumType>>::Type>;

      auto choose_message_type = [&]<typename T>() {
        auto type_correct_msg = std::static_pointer_cast<const T>(msg);

        return OnMessageHelper<INDEX>(
            derived->synchronizer.get(), type_correct_msg,
            [callback, derived](basis::core::MonotonicTime now, const T_DERIVED::Synchronizer::MessageSumType &msgs) {
              *callback = [derived, now, msgs]() { return derived->RunHandlerAndPublish(now, msgs).ToTopicMap(); };
            });
      };

      using Helper = VariantHelper<MaybeVariantMessageType>;

      // The alternate inproc type
      if constexpr (!std::is_same_v<typename Helper::inproc_type, core::transport::NoAdditionalInproc>) {
        if (type_name == T_DERIVED::subscription_inproc_message_type_names[INDEX]) {
          return choose_message_type.template operator()<typename Helper::inproc_type>();
        }
      }

      // The message type
      if (type_name == T_DERIVED::subscription_message_type_names[INDEX]) {
        return choose_message_type.template operator()<typename Helper::type>();
      }

      // Type mismatch, do nothing
      // This likely is okay, but let's warn for the short term
      // BASIS_LOG_WARN("Type mismatch between {} and {}{}, will not run type erased callback", type_name, T_DERIVED::subscription_message_type_names[INDEX], 
      // std::is_same_v<typename Helper::inproc_type, core::transport::NoAdditionalInproc> ? "", "/" + T_DERIVED::subscription_inproc_message_type_names[INDEX]
      // )
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
                  basis::core::containers::SubscriberQueueSharedPtr &subscriber_queue,
                  basis::core::threading::ThreadPool *thread_pool) {
    T_DERIVED *derived = ((T_DERIVED *)this);

    if (options.create_subscribers) {
      // First extract from the tuple of all input types
      using MaybeVariantMessageType = std::remove_const_t<typename basis::synchronizers::ExtractFromContainer<
          typename std::tuple_element_t<INDEX, typename T_DERIVED::Synchronizer::MessageSumType>>::Type>;
      // Now handle the fact that we might have std::variant<message type, inproc type>
      using MessageType = typename VariantHelper<MaybeVariantMessageType>::type;
      using MessageInprocType = typename VariantHelper<MaybeVariantMessageType>::inproc_type;

      auto subscriber_member_ptr = std::get<INDEX>(T_DERIVED::subscribers);

      constexpr bool is_raw = std::string_view(T_DERIVED::subscription_serializers[INDEX]) == "raw";
      using T_SERIALIZER = typename RawSerializationHelper<MessageType, is_raw>::type;

      derived->*subscriber_member_ptr = transport_manager->SubscribeCallable<MessageType, T_SERIALIZER, MessageInprocType>(
          derived->subscription_topics[INDEX], CreateOnMessageCallback<INDEX, false>(), thread_pool, subscriber_queue,
          T_SERIALIZER::template DeduceMessageTypeInfo<MessageType>(), CreateOnMessageCallback<INDEX, true>());
    }

    type_erased_callbacks[derived->subscription_topics[INDEX]] = CreateTypeErasedOnMessageCallback<INDEX>();
  }

  void SetupInputs(const basis::UnitInitializeOptions &options,
                   basis::core::transport::TransportManager *transport_manager,
                   std::array<basis::core::containers::SubscriberQueueSharedPtr, INPUT_COUNT> &subscriber_queues,
                   basis::core::threading::ThreadPool *thread_pool) {
    // Magic to iterate from 0...INPUT_COUNT, c++ really needs constexpr for
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (SetupInput<I>(options, transport_manager, subscriber_queues[I], thread_pool), ...);
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
                      [[maybe_unused]] const basis::core::Duration &max_execution_duration =
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
            basis::core::containers::SubscriberQueueSharedPtr output_queue = nullptr,
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

  virtual void Update(std::atomic<bool> *stop_token, const basis::core::Duration &max_execution_duration) override {
    basis::core::MonotonicTime update_until = basis::core::MonotonicTime::Now() + max_execution_duration;

    Unit::Update(stop_token, max_execution_duration);
    // TODO: this won't neccessarily sleep the max amount - this might be okay but could be confusing
    // try to get a single event, with a wait time
    if (auto event = overall_queue->Pop(max_execution_duration)) {
      (*event)();
    }

    // Try to drain the buffer of events
    while (auto event = overall_queue->Pop()) {
      (*event)();
      // TODO: this is somewhat of a kludge to rest of the Unit to Update() - we need to move towards a system where
      // those updates can happen
      if (update_until < basis::core::MonotonicTime::Now()) {
        break;
      }
    }

    // todo: it's possible that we may want to periodically schedule Update() for the output queue
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  [[nodiscard]] std::shared_ptr<core::transport::Subscriber<T_MSG>>
  Subscribe(std::string_view topic, core::transport::SubscriberCallback<T_MSG> callback, size_t queue_depth = 0,
            core::serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    return Unit::Subscribe<T_MSG, T_Serializer>(
        topic, callback, &thread_pool,
        std::make_shared<basis::core::containers::SubscriberQueue>(overall_queue, queue_depth),
        std::move(message_type));
  }

protected:
  std::shared_ptr<basis::core::containers::SubscriberOverallQueue> overall_queue =
      std::make_shared<basis::core::containers::SubscriberOverallQueue>();
  basis::core::threading::ThreadPool thread_pool{4};
};

// todo: interface for multithreaded units

} // namespace basis