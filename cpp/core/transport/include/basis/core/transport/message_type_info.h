#pragma once

#include <string>

namespace basis::core::transport {

struct MessageTypeInfo {
  std::string serializer;
  std::string id;
  size_t type_size = 0; // Required for raw types, to help ensure safety
};

/**
 * Construct the type information from the class. This needs some work around APIs to safely handle pointers, refs, etc.
 * This might still be allowed, but the API needs explored.
 *
 * TODO: move to a helper in serialization
 */
template <typename T> MessageTypeInfo DeduceMessageTypeInfo() {
  static_assert(!std::is_pointer<T>::value);
  return MessageTypeInfo("raw", typeid(typename std::decay<T>::type).name(), sizeof(T));
};

} // namespace basis::core::transport