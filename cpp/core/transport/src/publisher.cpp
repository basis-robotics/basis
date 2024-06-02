#include <unistd.h>

#include <basis/core/transport/publisher.h>
#include <uuid/uuid.h>

namespace basis::core::transport {

std::atomic<uint32_t> publisher_id_counter;

__uint128_t CreatePublisherId() {
  __uint128_t out;
  static_assert(sizeof(__uint128_t) == sizeof(char[16]));
  [[maybe_unused]] const int retval = uuid_generate_time_safe(reinterpret_cast<unsigned char *>(&out));
  /// @todo BASIS-16 not everything is mounted in the basis-env container that needs to be
  // Guard against uuid collisions
  // assert(retval == 0);
  return out;
}

PublisherInfo PublisherBase::GetPublisherInfo() {
  PublisherInfo out;
  out.publisher_id = publisher_id;
  out.topic = topic;
  out.schema_id = type_info.SchemaId();

  if (has_inproc) {
    out.transport_info["inproc"] = std::to_string(getpid());
  }
  for (auto &pub : transport_publishers) {
    out.transport_info[pub->GetTransportName()] = pub->GetConnectionInformation();
  }

  return out;
}
} // namespace basis::core::transport