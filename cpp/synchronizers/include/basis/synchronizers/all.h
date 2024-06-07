#pragma once

#include "synchronizer_base.h"

namespace basis::synchronizers {

template <typename... T_MSG_CONTAINERs> class All : public SynchronizerBase<T_MSG_CONTAINERs...> {
public:
  using Base = SynchronizerBase<T_MSG_CONTAINERs...>;
  using Base::SynchronizerBase;

protected:
  virtual bool IsReadyNoLock() override {
    // Maybe C++25 will have constexpr for loops on tuples
    return std::apply([](auto... x) { return (bool(x) && ...); }, Base::storage);
  }
};

} // namespace basis::synchronizers
