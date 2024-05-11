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
#if 0
/**
 * @brief PublisherBaseT
 * 
 * @tparam T_MSG - sererializable type to be published
 * @todo: this won't do
 */
template<typename T_MSG>
class PublisherBaseT : public PublisherBase {
public:
    PublisherBaseT() = default;
    virtual ~PublisherBaseT() = default;
    virtual void Publish(const T_MSG& data) = 0;

    // This needs to have an inproc inside of it to keep type safety
    // Inproc breaks the boundary between transport and serializer
};

#endif

class TransportPublisher {
public:
  virtual ~TransportPublisher() = default;
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