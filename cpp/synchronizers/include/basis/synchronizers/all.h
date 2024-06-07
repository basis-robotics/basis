#include "synchronizer_base.h"

namespace basis::synchronizers {


template <typename... T_MSGs> class All : public SynchronizerBase<T_MSGs...> {
public:
  using Base = SynchronizerBase<T_MSGs...>;
  using Base::SynchronizerBase;

protected:
  virtual bool IsReadyNoLock() override {

    // Maybe C++25 will have constexpr for loops on tuples
    return std::apply([](auto... x) { return (bool(x) && ...); }, Base::storage);
  }
};

} // namespace basis::synchronizers
