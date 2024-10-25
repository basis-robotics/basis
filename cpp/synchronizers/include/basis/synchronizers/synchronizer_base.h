#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <variant>

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

template <typename T> struct IsStdVariant : std::false_type {};

template <typename... Args> struct IsStdVariant<std::variant<Args...>> : std::true_type {};

template <class T> struct ExtractFromContainer {
  using Type = T;
};

template <class T>
  requires HasPushBack<T>
struct ExtractFromContainer<T> {
  using Type = T::value_type;
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
  // using T_MSG = ExtractMessageType<T_MSG_CONTAINER>::Type;
  MessageMetadata<T_MSG_CONTAINER> metadata;
  T_MSG_CONTAINER data;

  operator bool() const {
    if (metadata.is_optional) {
      return true;
    }
    if constexpr (HasPushBack<T_MSG_CONTAINER>) {
      return !data.empty();
    } else if constexpr (IsStdVariant<T_MSG_CONTAINER>::value) {
      return data.index() != 0;
    } else {
      return data != nullptr;
    }
  }

  void ApplyMessage(auto message) {
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
  using Callback = std::function<void(const basis::core::MonotonicTime &, T_MSG_CONTAINERs...)>;
  using MessageSumType = std::tuple<T_MSG_CONTAINERs...>;

  SynchronizerBase(MessageMetadata<T_MSG_CONTAINERs> &&...metadatas)
      : SynchronizerBase(std::forward_as_tuple(metadatas...)) {}

  SynchronizerBase(std::tuple<MessageMetadata<T_MSG_CONTAINERs>...> &&metadatas = {}) : storage(metadatas) {}

  virtual ~SynchronizerBase() = default;

  /**
   *
   * @tparam INDEX
   * @param msg
   * @param out Optional - to consume while still holding the lock, rather than as a separate call
   * @return bool
   */
  template <size_t INDEX> bool OnMessage(auto msg, MessageSumType *out = nullptr) {
    std::lock_guard lock(mutex);
    std::get<INDEX>(storage).ApplyMessage(msg);
    return PostApplyMessage(out);
  }

  std::optional<MessageSumType> ConsumeIfReady() {
    std::lock_guard lock(mutex);
    return ConsumeIfReadyNoLock();
  }
  bool IsReady() {
    std::lock_guard lock(mutex);
    return IsReadyNoLock();
  }

protected:
  bool PostApplyMessage(MessageSumType *out) {
    if (IsReadyNoLock()) {
      if (out) {
        *out = ConsumeMessagesNoLock();
      }
      return true;
    }
    return false;
  }
  std::optional<MessageSumType> ConsumeIfReadyNoLock() {
    if (IsReadyNoLock()) {
      MessageSumType out(ConsumeMessagesNoLock());

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

  std::tuple<Storage<T_MSG_CONTAINERs>...> storage;

  std::mutex mutex;
};

} // namespace basis::synchronizers
