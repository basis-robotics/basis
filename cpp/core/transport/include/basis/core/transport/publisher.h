namespace basis {
namespace core {
namespace transport {
/**
 * @brief PublisherBase - used to type erase Publisher
 * 
 */
class PublisherBase {
public:
    virtual ~PublisherBase() = default;
};

/**
 * @brief PublisherBaseT
 * 
 * @tparam T_MSG - sererializable type to be published
 */
template<typename T_MSG>
class PublisherBaseT : public PublisherBase {
public:
    PublisherBaseT() = default;
    virtual ~PublisherBaseT() = default;
    virtual void Publish(const T_MSG& data) = 0;
};
}
}
}