#pragma once

#include "synchronizer_base.h"

namespace basis::synchronizers {

template <typename... T_MSG_CONTAINERs> class All : public SynchronizerBase<T_MSG_CONTAINERs...> {
public:
  using Base = SynchronizerBase<T_MSG_CONTAINERs...>;
  using Base::Base;

protected:
  virtual bool IsReadyNoLock() override { return Base::AreAllNonOptionalFieldsFilledNoLock(); }
};

} // namespace basis::synchronizers
