#pragma once

#include <string_view>
#include <memory>
#include "inproc.h"
namespace basis {
namespace core {
namespace transport {
/**
 * @brief PublisherBase - used to type erase Publisher
 *
 */
class PublisherBase {
public:
  PublisherBase() = default;
  virtual ~PublisherBase() = default;
};


class TransportPublisher {
public:
  virtual ~TransportPublisher() = default;

};

template <typename T> class Publisher : public PublisherBase {
public:
  Publisher(std::vector<std::shared_ptr<TransportPublisher>> transport_publishers, std::shared_ptr<InprocPublisher<T>> inproc)
      :  inproc(inproc), transport_publishers(transport_publishers) {}
  
  std::shared_ptr<InprocPublisher<T>> inproc;
  std::vector<std::shared_ptr<TransportPublisher>> transport_publishers;
};

} // namespace transport
} // namespace core
} // namespace basis