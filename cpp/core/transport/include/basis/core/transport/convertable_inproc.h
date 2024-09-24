#pragma once

#include <memory>

namespace basis {

namespace MessageVariant {
  enum {
    NO_MESSAGE,
    TYPE_MESSAGE,
    INPROC_TYPE_MESSAGE,
  };
}


// Default conversion from struct to message, override if T_CONVERTABLE_INPROC isn't modifyable
template <typename T_MSG, typename T_CONVERTABLE_INPROC>
std::shared_ptr<const T_MSG> ConvertToMessage(const std::shared_ptr<const T_CONVERTABLE_INPROC>& in) {
  return in->ToMessage();
}
namespace core::transport {


class NoAdditionalInproc {

};
}

}