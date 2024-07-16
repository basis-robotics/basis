#include "basis/unit.h"

namespace basis {
std::unique_ptr<basis::core::transport::TransportManager> CreateStandardTransportManager(basis::RecorderInterface* recorder) {
  auto transport_manager = std::make_unique<basis::core::transport::TransportManager>(
      std::make_unique<basis::core::transport::InprocTransport>());

  if(recorder) {
    transport_manager->SetRecorder(recorder);
  }

  transport_manager->RegisterTransport(basis::plugins::transport::TCP_TRANSPORT_NAME,
                                        std::make_unique<basis::plugins::transport::TcpTransport>());

  return transport_manager;
}
}