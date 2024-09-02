#pragma once
#include "basis/core/coordinator_connector.h"
#include "basis/core/transport/publisher.h"
#include "basis/core/transport/transport_manager.h"

#include <basis/replayer/config.h>
#include <mcap/reader.hpp>
#include <memory>
#include <unordered_map>

#include <time.pb.h>

#include "replayer/logger.h"

namespace basis {
namespace core::transport {
    class TransportManager;
}
using namespace replayer;

class Replayer {
public:
  Replayer(Config config, 
    basis::core::transport::TransportManager &transport_manager,
    basis::core::transport::CoordinatorConnector& coordinator_connector)
      : config(std::move(config)), transport_manager(transport_manager), coordinator_connector(coordinator_connector) {
      }

  virtual bool Run();

  basis::core::MonotonicTime StartTime() {
    return basis::core::MonotonicTime::FromNanoseconds(mcap_reader.statistics()->messageStartTime);
  }
  basis::core::MonotonicTime EndTime() {
    return basis::core::MonotonicTime::FromNanoseconds(mcap_reader.statistics()->messageEndTime);
  }
protected:
  bool LoadRecording(std::filesystem::path recording_path);

  virtual bool OnRecordLoaded();

  const Config config;

  basis::core::transport::TransportManager &transport_manager;
  basis::core::transport::CoordinatorConnector& coordinator_connector;
  mcap::McapReader mcap_reader;
  std::unordered_map<std::string, std::shared_ptr<basis::core::transport::PublisherRaw>> publishers;
  std::shared_ptr<basis::core::transport::Publisher<basis::core::transport::proto::Time>> time_publisher;
};

} // namespace basis