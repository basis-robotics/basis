#include <unistd.h>

#include <uuid/uuid.h>
#include <basis/core/transport/publisher.h>

namespace basis::core::transport {

std::atomic<uint32_t> publisher_id_counter;

__uint128_t CreatePublisherId() {
  __uint128_t out;
  static_assert(sizeof(__uint128_t) == sizeof(char[16]));
  [[maybe_unused]] const int retval = uuid_generate_time_safe(reinterpret_cast<unsigned char*>(&out));
  // Guard against uuid collisions
  assert(retval == 0);
  return out;
}

  PublisherInfo PublisherBase::GetPublisherInfo() {
    PublisherInfo out;
    out.publisher_id = publisher_id;
    out.topic = topic;

    if(has_inproc) {
        out.transport_info["inproc"] = std::to_string(getpid());
    }
    for (auto &pub : transport_publishers) {
      out.transport_info[pub->GetTransportName()] = pub->GetConnectionInformation();
    }

    return out;
  }
}