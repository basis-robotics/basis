#pragma once

// this might be overdoing it

#include "synchronizer_base.h"
#include <type_traits>
#include <spdlog/spdlog.h>
namespace basis::synchronizers {

template <typename T_CONTAINER_MSG, auto T_FIELD> struct Field {};

namespace internal {
// Helper meta-function to extract the Container type from Field
template <typename T> struct ExtractFieldContainer;

template <template <typename, auto> typename T_FIELD_SYNC, typename T_CONTAINER, auto T_FIELD>
struct ExtractFieldContainer<T_FIELD_SYNC<T_CONTAINER, T_FIELD>> {
  using Type = T_CONTAINER;
};

template <typename T> struct ExtractField;
template <template <typename, auto> typename T_FIELD_SYNC, typename T_CONTAINER, auto T_FIELD>
struct ExtractField<T_FIELD_SYNC<T_CONTAINER, T_FIELD>> {
  constexpr static auto Field = T_FIELD;
};

template <typename... T_FIELD_SYNCs>
concept NoContainerSupportForFieldSync =
    ((internal::ExtractField<T_FIELD_SYNCs>::Field == nullptr ||
      !HasPushBack<typename internal::ExtractFieldContainer<T_FIELD_SYNCs>::Type>) &&
     ...);

} // namespace internal

template <typename... T_FIELD_SYNCs>
// This class does not support vectors for synced messages
  requires internal::NoContainerSupportForFieldSync<T_FIELD_SYNCs...>
class FieldSync : public SynchronizerBase<typename internal::ExtractFieldContainer<T_FIELD_SYNCs>::Type...> {
public:
  // static_assert(typename internal::ExtractFieldContainer<T_FIELD_SYNCs>::Type... , "all numbers must be 0 or 1");

  using Base = SynchronizerBase<typename internal::ExtractFieldContainer<T_FIELD_SYNCs>::Type...>;

  using MessageSumType = Base::MessageSumType;

protected:
  virtual bool IsReadyNoLock() override { return false; }

  template <auto T_INDIR_B, typename T_FIELD_A, typename T_MSG_B>
  int FindMatchingField(T_FIELD_A &a, const std::vector<T_MSG_B> &syncs_b) {
    // TODO: we can do a binary search here...maybe
    if constexpr (T_INDIR_B != nullptr) {
      for (size_t i = 0; i < syncs_b.size(); i++) {
        auto *b = syncs_b[i].get();
        // Handle protobuf not exposing members directly
        if constexpr (std::is_member_function_pointer_v<decltype(T_INDIR_B)>) {
          if ((b->*T_INDIR_B)() == a) {
            return i;
          }
        } else {
          if (b->*T_INDIR_B == a) {
            return i;
          }
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

    if constexpr (pointer_to_member) {
      std::get<INDEX>(sync_buffers).push_back(msg);

      MessageSumType matching;

      [[maybe_unused]] const auto &field_to_check = msg.get()->*pointer_to_member;

      [&]<std::size_t... I>(std::index_sequence<I...>) {
        const auto syncs =
            std::tuple(FindMatchingField<std::get<I>(fields)>(field_to_check, std::get<I>(sync_buffers))...);
        is_synced = ((std::get<I>(fields) == nullptr || std::get<I>(syncs) != -1) && ...);
        if (is_synced) {
          [[maybe_unused]] auto t = ((ApplySync<I>(std::get<I>(syncs)), true) && ...);
        }
        return is_synced;
      }(std::index_sequence_for<T_FIELD_SYNCs...>());

    } else {
      std::get<INDEX>(this->storage).ApplyMessage(msg);
    }
spdlog::info("foobar");
    return this->ConsumeIfReadyNoLock();
  }

protected:
  bool is_synced = false;

  static constexpr auto fields = (std::make_tuple(internal::ExtractField<T_FIELD_SYNCs>::Field...));

  std::tuple<std::vector<typename internal::ExtractFieldContainer<T_FIELD_SYNCs>::Type>...> sync_buffers;
};

} // namespace basis::synchronizers