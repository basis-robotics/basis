#pragma once
#include <basis/core/coordinator_connector.h>
#include <basis/core/threading/thread_pool.h>
#include <basis/core/transport/transport_manager.h>

namespace basis {

class Unit {
public:
  Unit(/*std::string_view unit_name*/) {

  }

  void WaitForCoordinatorConnection() {
     while (!coordinator_connector) {
        coordinator_connector = basis::core::transport::CoordinatorConnector::Create();
        if (!coordinator_connector) {
          spdlog::warn("No connection to the coordinator, waiting 1 second and trying again");
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
      }
  }

  void CreateTransportManager() {
    // todo: it may be better to pass these in - do we want one transport manager per unit ?
    // probably yes, so that they each get an ID

    transport_manager = std::make_unique<basis::core::transport::TransportManager>(
        std::make_unique<basis::core::transport::InprocTransport>());

    transport_manager->RegisterTransport(basis::plugins::transport::TCP_TRANSPORT_NAME,
                                         std::make_unique<basis::plugins::transport::TcpTransport>());
  }

  // override this, should be called once by main()
  virtual void Initialize() = 0;

  virtual ~Unit() = default;
  virtual void Update() {
    transport_manager->Update();
    // send it off to the coordinator
    if (coordinator_connector) {
      std::vector<basis::core::serialization::MessageSchema> new_schemas =
          transport_manager->GetSchemaManager().ConsumeSchemasToSend();
      if (new_schemas.size()) {
        coordinator_connector->SendSchemas(new_schemas);
      }
      coordinator_connector->SendTransportManagerInfo(transport_manager->GetTransportManagerInfo());
      coordinator_connector->Update();

      if (coordinator_connector->GetLastNetworkInfo()) {
        transport_manager->HandleNetworkInfo(*coordinator_connector->GetLastNetworkInfo());
      }
    }
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
  std::unique_ptr<basis::core::transport::TransportManager> transport_manager;
  std::unique_ptr<basis::core::transport::CoordinatorConnector> coordinator_connector;
};

/**
 * A simple unit where all are run mutally exclusive from eachother - uses a queue for all outputs, which adds some
 * amount of latency
 */
class SingleThreadedUnit : public Unit {
protected:
  using Unit::Update;

public:
  using Unit::Advertise;
  using Unit::Initialize;
  using Unit::Unit;

  void Update(int sleep_time_s) {
    Update();

    // try to get a single event, with a wait time
    if (auto event = output_queue.Pop(sleep_time_s)) {
      (*event)();
    }

    // Try to drain the buffer of events
    while (auto event = output_queue.Pop(0)) {
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