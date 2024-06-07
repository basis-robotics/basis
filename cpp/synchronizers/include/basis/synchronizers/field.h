#pragma once

// this might be overdoing it

#include "synchronizer_base.h"
#include <spdlog/spdlog.h>
#include <type_traits>
namespace basis::synchronizers {

/**
 * Helper to associate a message type and a field in that message
 * @tparam T_CONTAINER_MSG - the container for the message - eg std::shared_ptr<Message>
 * @tparam T_FIELD - a pointer to member for the field to be symced (or nullptr if unsynced).
 *  May be a pointer to the actual field or pointer to member taking no additional arguments.
 */
template <typename T_CONTAINER_MSG, auto T_FIELD> struct Field {
  using ContainerMessageType = T_CONTAINER_MSG;
  static constexpr auto FieldPtr = T_FIELD;
};

namespace internal {
/**
 * (internal) Helper to ensure that we don't sync a field of a container that's buffered.
 */
template <typename... T_FIELD_SYNCs>
concept NoContainerSupportForFieldSync =
    ((T_FIELD_SYNCs::FieldPtr == nullptr ||
      !HasPushBack<typename T_FIELD_SYNCs::ContainerMessageType>) &&
     ...);

} // namespace internal

/**
 * Class used to synchronize fields in messages.
 * @tparam T_OPERATOR class used to check if two fields are synced. Must be a class with this structure:       
    struct Operator {
        template<typename T1, typename T2>
        auto operator()(const T1& t1, const T2& t2) {
            return MyOperation(t1, t2); // (eg, t1 == t2)
        }
    };
 * @tparam T_FIELD_SYNCs A parameter packed list of types and fields to sync, eg:
    basis::synchronizers::Field<std::shared_ptr<const SensorMessages::LidarScan>, &SensorMessages::LidarScan::header.timestamp>,
    basis::synchronizers::Field<std::shared_ptr<const TestProtoStruct>, &TestProtoStruct::foo>,
 */
template <typename T_OPERATOR, typename... T_FIELD_SYNCs>
// This class does not support vectors for synced messages
  requires internal::NoContainerSupportForFieldSync<T_FIELD_SYNCs...>
class FieldSync : public SynchronizerBase<typename T_FIELD_SYNCs::ContainerMessageType...> {
public:
  using Base = SynchronizerBase<typename T_FIELD_SYNCs::ContainerMessageType...>;

  using MessageSumType = Base::MessageSumType;

  template <auto T_INDIR_A, typename T_MSG_A> auto GetFieldData(T_MSG_A a) {
    if constexpr (std::is_member_function_pointer_v<decltype(T_INDIR_A)>) {
      return (a->*T_INDIR_A)();
    } else {
      return a->*T_INDIR_A;
    }
  }

protected:
  virtual bool IsReadyNoLock() override {
    return Base::AreAllNonOptionalFieldsFilledNoLock();
  }

  template <auto T_INDIR_B, typename T_FIELD_A, typename T_MSG_B>
  int FindMatchingField(T_FIELD_A &a, const std::vector<T_MSG_B> &syncs_b) {
    // TODO: we can do a binary search here...maybe
    if constexpr (T_INDIR_B != nullptr) {
      for (size_t i = 0; i < syncs_b.size(); i++) {
        auto *b = syncs_b[i].get();
        // Handle protobuf not exposing members directly
        if (T_OPERATOR()(GetFieldData<T_INDIR_B>(b), a)) {
          return i;
        }
      }
    }

    return -1;
  }

  template <auto INDEX> void ApplySync(size_t sync_index) {
    if constexpr (std::get<INDEX>(fields)) {
      auto &sync = std::get<INDEX>(sync_buffers);
      std::get<INDEX>(this->storage).ApplyMessage(sync[sync_index]);
      sync.erase(sync.begin(), sync.begin() + sync_index + 1);
    }
  }

public:
  template <size_t INDEX> std::optional<MessageSumType> OnMessage(auto msg) {
    std::lock_guard lock(this->mutex);

    constexpr auto pointer_to_member = std::get<INDEX>(fields);

    if constexpr (pointer_to_member != nullptr) {
      std::get<INDEX>(sync_buffers).push_back(msg);

      MessageSumType matching;

      auto field_to_check = GetFieldData<pointer_to_member>(msg.get());

      [&]<std::size_t... I>(std::index_sequence<I...>) {
        const auto syncs =
            std::tuple(FindMatchingField<std::get<I>(fields)>(field_to_check, std::get<I>(sync_buffers))...);
        const bool is_synced = ((std::get<I>(fields) == nullptr || std::get<I>(syncs) != -1) && ...);
        if (is_synced) {
          [[maybe_unused]] auto t = ((ApplySync<I>(std::get<I>(syncs)), true) && ...);
        }
      }(std::index_sequence_for<T_FIELD_SYNCs...>());

    } else {
      std::get<INDEX>(this->storage).ApplyMessage(msg);
    }

    return this->ConsumeIfReadyNoLock();
  }

protected:
  static constexpr auto fields = (std::make_tuple(T_FIELD_SYNCs::FieldPtr...));

  std::tuple<std::vector<typename T_FIELD_SYNCs::ContainerMessageType>...> sync_buffers;
};

struct Equal {
    template<typename T1, typename T2>
    auto operator()(const T1& t1, const T2& t2) {
        return t1 == t2;
    }
};

template<auto T_EPSILON>
struct ApproximatelyEqual {
    template<typename T1, typename T2>
    auto operator()(const T1& t1, const T2& t2) {
        return std::abs(t1 - t2) <= T_EPSILON;
    }
};

template <typename... T_FIELD_SYNCs>
using FieldSyncEqual = FieldSync<Equal, T_FIELD_SYNCs...>;

} // namespace basis::synchronizers