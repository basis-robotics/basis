#pragma once

#include <argparse/argparse.hpp>
// TODO: we can factor the json dependency out into a cpp file, with some work, to speed up compilation
#include <nlohmann/json.hpp>

namespace basis::arguments::types {
using namespace std;

// TODO: this is basically replicating what virtual functions do - is it cleaner or dirtier?
struct TypeMetadata {
  std::string type_name;
  std::function<void(argparse::Argument &, std::string_view)> validator;
  std::function<void(const argparse::ArgumentParser &, std::string_view, nlohmann::json &)> to_json;
};

template <typename T_ARGUMENT_TYPE>
void ArgumentToJson(const argparse::ArgumentParser &arg_parser, std::string_view name, nlohmann::json &out) {
  out = arg_parser.get<T_ARGUMENT_TYPE>("--" + std::string(name));
}

template <typename T_ARGUMENT_TYPE> void ArgumentTypeValidator(argparse::Argument &arg, std::string_view name) {
  if constexpr (std::is_same_v<T_ARGUMENT_TYPE, bool>) {
    arg.action([name = std::string(name)](const std::string &value) {
      std::string lowered = value;
      std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return std::tolower(c); });
      if (lowered == "true" or lowered == "1") {
        return true;
      }
      if (lowered == "false" or lowered == "0") {
        return false;
      }
      throw std::runtime_error(
          fmt::format("[--{} {}] can't be converted to bool, must be '0', '1', 'true', or 'false' (case insensitive)",
                      name, value));
    });
  } else if constexpr (std::is_floating_point_v<T_ARGUMENT_TYPE>) {
    arg.template scan<'g', T_ARGUMENT_TYPE>();
  } else if constexpr (std::is_arithmetic_v<T_ARGUMENT_TYPE>) {
    arg.template scan<'i', T_ARGUMENT_TYPE>();
  }
}

// X Macros are great - they allow declaring a list once, and reusing it with the preprocessor multiple times
// https://en.wikipedia.org/wiki/X_macro

// The types we allow for launch files and unit files
#define X_ALLOWED_ARGUMENT_TYPES                                                                                       \
  X_TYPE(bool)                                                                                                         \
  X_TYPE(string)                                                                                                       \
  X_TYPE(uint8_t)                                                                                                      \
  X_TYPE(int8_t)                                                                                                       \
  X_TYPE(uint16_t)                                                                                                     \
  X_TYPE(int16_t)                                                                                                      \
  X_TYPE(uint32_t)                                                                                                     \
  X_TYPE(int32_t)                                                                                                      \
  X_TYPE(uint64_t)                                                                                                     \
  X_TYPE(int64_t)                                                                                                      \
  X_TYPE(float)                                                                                                        \
  X_TYPE(double)

// Helper to construct a TypeMetadata from a type
#define DECLARE_ARGUMENT_TYPE(type)                                                                                    \
  {.type_name = #type, .validator = &ArgumentTypeValidator<type>, .to_json = &ArgumentToJson<type>}

// Turn the undefined x macro into a defined x macro, mapping name to TypeMetadata
#define X_TYPE(type) {#type, DECLARE_ARGUMENT_TYPE(type)},

// Declare full map of name to TypeMetadata
inline const std::unordered_map<std::string, TypeMetadata> allowed_argument_types = {X_ALLOWED_ARGUMENT_TYPES};

#undef X_TYPE

// Helper to map an instantiated type to TypeMetadata
template <typename T> struct TypeToTypeMetadata {};

// now turn the x macro into overrides of the helper class
#define X_TYPE(type)                                                                                                   \
  template <> struct TypeToTypeMetadata<type> {                                                                        \
    const inline static TypeMetadata metadata DECLARE_ARGUMENT_TYPE(type);                                             \
  };

// Instantiate the x macro
X_ALLOWED_ARGUMENT_TYPES

#undef X_TYPE
#undef X_ALLOWED_ARGUMENT_TYPES
#undef DECLARE_ARGUMENT_TYPE

} // namespace basis::arguments::types