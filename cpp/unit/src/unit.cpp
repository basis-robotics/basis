#include "basis/unit.h"

namespace basis {
std::unique_ptr<basis::core::transport::TransportManager>
CreateStandardTransportManager(basis::RecorderInterface *recorder) {
  auto transport_manager = std::make_unique<basis::core::transport::TransportManager>(
      std::make_unique<basis::core::transport::InprocTransport>());

  if (recorder) {
    transport_manager->SetRecorder(recorder);
  }

  transport_manager->RegisterTransport(basis::plugins::transport::TCP_TRANSPORT_NAME,
                                       std::make_unique<basis::plugins::transport::TcpTransport>());

  return transport_manager;
}

// TODO: this is purely concerned with transport concepts, we should consider moving this to transport
void StandardUpdate(basis::core::transport::TransportManager *transport_manager,
                    basis::core::transport::CoordinatorConnector *coordinator_connector) {
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

} // namespace basis