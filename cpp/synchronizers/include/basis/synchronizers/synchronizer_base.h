#include <functional>
#include <memory>
#include <mutex>
#include <tuple>

namespace basis::synchronizers {

template <typename T_MSG> struct MessageMetadata {
  using MessageType = T_MSG;
  /**
   * If set, this message will not participate in ready checks, but will still be passed into any results.
   */
  bool is_optional = false;
  /**
   * If set, this message will not be cleared when complete.
   */
  bool is_cached = false;
  
};

template <typename... T_MSGs> class SynchronizerBase {
public:
  using Callback = std::function<void(std::shared_ptr<T_MSGs>...)>;
  using MessageSumType = std::tuple<std::shared_ptr<T_MSGs>...>;
  SynchronizerBase(Callback callback, MessageMetadata<T_MSGs> &&...metadatas)
      : SynchronizerBase(callback, std::forward_as_tuple(metadatas...)) {}

  SynchronizerBase(Callback callback = {}, std::tuple<MessageMetadata<T_MSGs>...> &&metadatas = {})
      : callback(callback), storage(metadatas) {}

  template <size_t INDEX, typename T> MessageSumType OnMessage(std::shared_ptr<T> msg) {
    std::lock_guard lock(mutex);
    std::get<INDEX>(storage).message = msg;

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
    return std::apply([](auto&... storage) {
        return std::tuple{storage.Consume()...};
    }, storage);
  }

  Callback callback;

  template <typename T_MSG> struct Storage {
    MessageMetadata<T_MSG> metadata;
    std::shared_ptr<T_MSG> message;

    operator bool() { return metadata.is_optional || message; }

    std::shared_ptr<T_MSG> Consume() {
      if(metadata.is_cached) {
        return message;
      }
      else {
        return std::move(message);
      }
    }
  };

  std::tuple<Storage<T_MSGs>...> storage;

  std::mutex mutex;
};

} // namespace basis::synchronizers
