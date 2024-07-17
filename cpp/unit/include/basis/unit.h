#pragma once
#include <basis/core/coordinator_connector.h>
#include <basis/core/logging.h>
#include <basis/core/threading/thread_pool.h>
#include <basis/core/transport/transport_manager.h>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace basis {

std::unique_ptr<basis::core::transport::TransportManager>
CreateStandardTransportManager(basis::RecorderInterface *recorder = nullptr);

void StandardUpdate(basis::core::transport::TransportManager *transport_manager,
                    basis::core::transport::CoordinatorConnector *coordinator_connector);

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
  virtual void Initialize() = 0;

  virtual void Update([[maybe_unused]] const basis::core::Duration &max_sleep_duration =
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
            basis::core::threading::ThreadPool *work_thread_pool, core::transport::OutputQueue *output_queue = nullptr,
            core::serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    return transport_manager->Subscribe<T_MSG, T_Serializer>(topic, std::move(callback), work_thread_pool, output_queue,
                                                             std::move(message_type));
  }

protected:
  std::string unit_name;
  std::shared_ptr<spdlog::logger> logger;
  const std::shared_ptr<spdlog::logger> &AUTO_LOGGER = logger;
  std::unique_ptr<basis::core::transport::TransportManager> transport_manager;
  std::unique_ptr<basis::core::transport::CoordinatorConnector> coordinator_connector;
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

  virtual void Update(const basis::core::Duration &max_sleep_duration) override {
    Unit::Update(max_sleep_duration);
    // TODO: this won't neccessarily sleep the max amount - this might be okay but could be confusing

    // try to get a single event, with a wait time
    if (auto event = output_queue.Pop(max_sleep_duration)) {
      (*event)();
    }

    // Try to drain the buffer of events
    while (auto event = output_queue.Pop()) {
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

  basis::core::transport::OutputQueue output_queue;
  basis::core::threading::ThreadPool thread_pool{4};
};

// todo: interface for multithreaded units

} // namespace basis