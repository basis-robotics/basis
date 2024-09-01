#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <tuple>

#include <basis/core/time.h>

namespace basis::synchronizers {

// todo: how are we going to add timing require to this

template <typename T_MSG_CONTAINER> struct MessageMetadata {
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

template <typename T>
concept HasPushBack = requires {
  { std::declval<T>().push_back(std::declval<typename T::value_type>()) } -> std::same_as<void>;
};

template <class T> struct ExtractMessageType;

template <class T>
  requires HasPushBack<T>
struct ExtractMessageType<T> {
  using Type = T::value_type::element_type;
};

template <class T>
  requires requires { typename T::element_type; }
struct ExtractMessageType<T> {
  using Type = T::element_type;
};

// template

// template<class T>
// struct ExtractMessageType {
//   using Type = HasPushBack<T> ? T::value_type::element_type : T::element_type;
// };

template <typename T_MSG_CONTAINER> struct Storage {
  using T_MSG = ExtractMessageType<T_MSG_CONTAINER>::Type;
  MessageMetadata<T_MSG_CONTAINER> metadata;
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

// "base" type to use for holding inside a smart ptr
class Synchronizer {
public:
  virtual ~Synchronizer() = default;
};

template <typename... T_MSG_CONTAINERs> class SynchronizerBase : Synchronizer {
public:
  using Callback = std::function<void(const basis::core::MonotonicTime&, T_MSG_CONTAINERs...)>;
  using MessageSumType = std::tuple<T_MSG_CONTAINERs...>;

  SynchronizerBase(Callback callback, MessageMetadata<T_MSG_CONTAINERs> &&...metadatas)
      : SynchronizerBase(callback, std::forward_as_tuple(metadatas...)) {}

  SynchronizerBase(Callback callback = {}, std::tuple<MessageMetadata<T_MSG_CONTAINERs>...> &&metadatas = {})
      : callback(callback), storage(metadatas) {}

  virtual ~SynchronizerBase() = default;

  template <size_t INDEX> void OnMessage(auto msg) {
    std::lock_guard lock(mutex);
    std::get<INDEX>(storage).ApplyMessage(msg);
  }

  std::optional<MessageSumType> ConsumeIfReady(const basis::core::MonotonicTime& now) {
    std::lock_guard lock(mutex);
    return ConsumeIfReadyNoLock(now);
  }
  bool IsReady() {
    std::lock_guard lock(mutex);
    return IsReadyNoLock();
  }
protected:
  std::optional<MessageSumType> ConsumeIfReadyNoLock(const basis::core::MonotonicTime& now) {
    if (IsReadyNoLock()) {
      MessageSumType out(ConsumeMessagesNoLock());

      if (callback) {
        // std::apply doesn't allow prepending arguments, so sneak `now` in through a lambda  
        auto f = [&]<typename... Ts>(Ts&&... ts){
          return callback(now, std::forward<Ts>(ts)...);
        };
        std::apply(f, out);
      }
      return out;
    }
    return {};
  }

  virtual bool IsReadyNoLock() = 0;

  MessageSumType ConsumeMessagesNoLock() {
    return std::apply([](auto &...storage) { return std::tuple{storage.Consume()...}; }, storage);
  }

  bool AreAllNonOptionalFieldsFilledNoLock() {
    // Maybe C++25 will have constexpr for loops on tuples
    return std::apply([](auto... x) { return (bool(x) && ...); }, storage);
  }

  Callback callback;

  std::tuple<Storage<T_MSG_CONTAINERs>...> storage;

  std::mutex mutex;
};

} // namespace basis::synchronizers
