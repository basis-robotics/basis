#include <functional>
#include <memory>
#include <mutex>
#include <tuple>

namespace basis::synchronizers {

struct MessageMetadata {
  /**
   * If set, this message will not participate in ready checks, but will still be passed into any results.
   */
  bool is_optional = false;
  /**
   * If set, this message will not be cleared when complete.
   */
  bool is_cached = false;

  // todo: min/max size
  // todo: we've untemplated this but maybe we should retemplate it?
};

/**
 * Helper class that always returns MessageMetadata for any template type
 */
template <typename T_MSG_CONTAINERs> struct MessageMetadataHelper {
  using Type = MessageMetadata;
};
template<typename T>
concept HasPushBack = requires {
    { std::declval<T>().push_back(std::declval<typename T::value_type>()) } -> std::same_as<void>;
};



template<class T>
struct ExtractMessageType {
};

template<class T>
  requires requires { typename T::element_type; }
struct ExtractMessageType<T> {
  using Type = T::element_type;
};
template<class T>
  requires HasPushBack<T>
struct ExtractMessageType<T> {
  using Type = T::value_type::element_type;
};

// template

// template<class T>
// struct ExtractMessageType {
//   using Type = HasPushBack<T> ? T::value_type::element_type : T::element_type;
// };


template <typename T_MSG_CONTAINER> struct Storage {
  using T_MSG = ExtractMessageType<T_MSG_CONTAINER>::Type;
  MessageMetadata metadata;
  T_MSG_CONTAINER data;

  operator bool() const {
    if (metadata.is_optional) {
      return true;
    }
    if constexpr (HasPushBack<T_MSG_CONTAINER>) {
      return !data.empty();
    } else {
      return data != nullptr;
    }
  }

  void ApplyMessage(std::shared_ptr<T_MSG> message) {
    if constexpr (HasPushBack<T_MSG_CONTAINER>) {
      data.push_back(message);
    } else {
      data = message;
    }
  }

  T_MSG_CONTAINER Consume() {
    if (metadata.is_cached) {
      return data;
    } else {
      return std::move(data);
    }
  }
};

template <typename... T_MSG_CONTAINERs> class SynchronizerBase {
public:
  using Callback = std::function<void(T_MSG_CONTAINERs...)>;
  using MessageSumType = std::tuple<T_MSG_CONTAINERs...>;

  SynchronizerBase(Callback callback, MessageMetadataHelper<T_MSG_CONTAINERs>::Type &&...metadatas)
      : SynchronizerBase(callback, std::forward_as_tuple(metadatas...)) {}

  SynchronizerBase(Callback callback = {},
                   std::tuple<typename MessageMetadataHelper<T_MSG_CONTAINERs>::Type...> &&metadatas = {})
      : callback(callback), storage(metadatas) {}

  template <size_t INDEX> MessageSumType OnMessage(auto msg) {
    std::lock_guard lock(mutex);
    std::get<INDEX>(storage).ApplyMessage(msg);

    MessageSumType out;

    if (IsReadyNoLock()) {
      out = ConsumeMessages();
      if (callback) {
        std::apply(callback, out);
      }
      return out;
    }
    return out;
  }

protected:
  virtual bool IsReadyNoLock() = 0;

protected:
  MessageSumType ConsumeMessages() {
    return std::apply([](auto &...storage) { return std::tuple{storage.Consume()...}; }, storage);
  }

  Callback callback;

  std::tuple<Storage<T_MSG_CONTAINERs>...> storage;

  std::mutex mutex;
};

} // namespace basis::synchronizers
