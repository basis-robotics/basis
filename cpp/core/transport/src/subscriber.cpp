#include <basis/core/transport/logger.h>
#include <basis/core/transport/subscriber.h>

#include <spdlog/spdlog.h>

#include <unistd.h>

namespace basis::core::transport {
void SubscriberBase::HandlePublisherInfo(const std::vector<PublisherInfo> &info) {
  for (const PublisherInfo &publisher_info : info) {
    if (publisher_info.topic != topic) {
      BASIS_LOG_ERROR("Skipping publisher info because no topic");
      continue;
    }
    const __uint128_t &publisher_id = publisher_info.publisher_id;
    auto it = publisher_id_to_transport_sub.find(publisher_id);

    if (it != publisher_id_to_transport_sub.end()) {
      if (it->second == nullptr) {
        // Inproc is always valid
        continue;
      }

/// @todo BASIS-11: need to be able to handle disconnects
#if 0
        if(it->second->IsConnectedToPublisher(publisher_id)) {
          continue;
        }
#else
      continue;
#endif
    }

    // todo check if inproc pid is ours
    if (has_inproc) {
      auto it = publisher_info.transport_info.find("inproc");
      if (it != publisher_info.transport_info.end() && it->second == std::to_string(getpid())) {
        // No need to do anything
        publisher_id_to_transport_sub.emplace(publisher_id, nullptr);
        continue;
      }
    }

    /// @todo BASIS-12: this will arbitrarily pick a transport once we add another type
    for (auto &transport_subscriber : transport_subscribers) {
      for (const auto &[pub_transport_name, pub_transport_endpoint] : publisher_info.transport_info) {
        if (pub_transport_name == transport_subscriber->GetTransportName()) {
          if (transport_subscriber->Connect("127.0.0.1", pub_transport_endpoint, publisher_id)) {
            publisher_id_to_transport_sub.emplace(publisher_id, transport_subscriber.get());
          }
        }
      }
    }
  }
}

size_t SubscriberBase::GetPublisherCount() {
  size_t count = 0;
  for (auto &ts : transport_subscribers) {
    count += ts->GetPublisherCount();
  }
  return count;
}
} // namespace basis::core::transport