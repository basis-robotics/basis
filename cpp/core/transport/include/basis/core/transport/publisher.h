#pragma once
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

  virtual std::shared_ptr<basis::core::transport::TransportPublisher> Advertise(std::string_view topic, MessageTypeInfo type_info) = 0;

};

template <typename T> class Publisher : public PublisherBase {
public:
  Publisher(std::vector<std::shared_ptr<TransportPublisher>> transport_publishers)
      : transport_publishers(transport_publishers) {}
  std::vector<std::shared_ptr<TransportPublisher>> transport_publishers;
};

} // namespace transport
} // namespace core
} // namespace basis