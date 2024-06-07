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
    ((T_FIELD_SYNCs::FieldPtr == nullptr || !HasPushBack<typename T_FIELD_SYNCs::ContainerMessageType>) && ...);

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
    basis::synchronizers::Field<std::shared_ptr<const SensorMessages::LidarScan>,
      [](const SensorMessages::LidarScan *scan) { return scan->header().stamp();>,
    basis::synchronizers::Field<std::shared_ptr<const Localization>, &Localization::stamp>,
    basis::synchronizers::Field<std::shared_ptr<const MapData>, nullptr

    This parameter can also be used for message conversions
 */
template <typename T_OPERATOR, typename... T_FIELD_SYNCs>
// This class does not support vectors for synced messages
  requires internal::NoContainerSupportForFieldSync<T_FIELD_SYNCs...>
class FieldSync : public SynchronizerBase<typename T_FIELD_SYNCs::ContainerMessageType...> {
public:
  using Base = SynchronizerBase<typename T_FIELD_SYNCs::ContainerMessageType...>;
  using Base::Base;
  using MessageSumType = Base::MessageSumType;

  /**
   * Handles a message at INDEX. Will call the owned callback if all messages are ready.
   * @tparam INDEX
   * @param msg
   * @return the set of messages that would be called using the callback
   */
  template <size_t INDEX> std::optional<MessageSumType> OnMessage(auto msg) {
    std::lock_guard lock(this->mutex);

    constexpr auto pointer_to_member = std::get<INDEX>(fields);

    if constexpr (pointer_to_member != nullptr) {
      std::get<INDEX>(sync_buffers).push_back(msg);

      MessageSumType matching;

      auto field_to_check = GetFieldData<pointer_to_member>(msg.get());

      [&]<std::size_t... I>(std::index_sequence<I...>) {
        const auto syncs =
            std::tuple(FindMatchingFieldNoLock<std::get<I>(fields)>(field_to_check, std::get<I>(sync_buffers))...);
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
  /**
   * Handle both pointer to member and function access to a field on a member
   * @tparam T_FIELD_ACCESS either pointer to member or function that takes in a T_MSG_A
   * @tparam T_MSG_A (usually deduced) type of the message to extract from
   * @param msg the message to extract from
   * @return the returned value
   */
  template <auto T_FIELD_ACCESS, typename T_MSG> auto GetFieldData(T_MSG msg) const {
    if constexpr (std::is_invocable_v<decltype(T_FIELD_ACCESS), T_MSG>) {
      return std::invoke(T_FIELD_ACCESS, msg);
    } else {
      return msg->*T_FIELD_ACCESS;
    }
  }

  /**
   * Checks if all fields are populated.
   */
  virtual bool IsReadyNoLock() override { return Base::AreAllNonOptionalFieldsFilledNoLock(); }

  /**
   * Tries to match all stored values for B against value A
   * @tparam T_FIELD_ACCESS_B the field accessor for B
   * @tparam T_VALUE_A (deduced) the type of value_a
   * @tparam T_MSG_B (deduced) the type message to access in B
   * @param value_a 
   * @param syncs_b
   * @return -1 if not found or not synced, otherwise the index of the field
   * @todo reverse A and B
   * @todo just take INDEX as the template argument
   */
  template <auto T_FIELD_ACCESS_B, typename T_VALUE_A, typename T_MSG_B>
  int FindMatchingFieldNoLock(T_VALUE_A &value_a, const std::vector<T_MSG_B> &syncs_b) {
    // TODO: we can do a binary search here...maybe
    
    // Ignore unsynced fields
    if constexpr (T_FIELD_ACCESS_B != nullptr) {
      // Search for a B that matches A
      for (size_t i = 0; i < syncs_b.size(); i++) {
        // Call T_OPERATOR on the field for this B against A
        if (T_OPERATOR()(GetFieldData<T_FIELD_ACCESS_B>(syncs_b[i].get()), value_a)) {
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

  static constexpr auto fields = (std::make_tuple(T_FIELD_SYNCs::FieldPtr...));

  std::tuple<std::vector<typename T_FIELD_SYNCs::ContainerMessageType>...> sync_buffers;
};

struct Equal {
  template <typename T1, typename T2> auto operator()(const T1 &t1, const T2 &t2) { return t1 == t2; }
};
template <typename... T_FIELD_SYNCs> using FieldSyncEqual = FieldSync<Equal, T_FIELD_SYNCs...>;

template <auto EPSILON> struct ApproximatelyEqual {
  template <typename T1, typename T2> auto operator()(const T1 &t1, const T2 &t2) {
    return std::abs(t1 - t2) <= EPSILON;
  }
};

template <auto EPSILON, typename... T_FIELD_SYNCs> using FieldSyncApproximatelyEqual = FieldSync<ApproximatelyEqual<EPSILON>, T_FIELD_SYNCs...>;

} // namespace basis::synchronizers