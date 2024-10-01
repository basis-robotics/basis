#include "argparse/argparse.hpp"
#include <basis/arguments/argument_types.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace basis::arguments::types {
using namespace std;
template <typename T_ARGUMENT_TYPE>
void ArgumentToJson(const argparse::ArgumentParser &arg_parser, std::string_view name, bool is_optional,
                    nlohmann::json &out) {
  std::string key = "--" + std::string(name);

  if (is_optional) {
    std::optional<T_ARGUMENT_TYPE> maybe_value = arg_parser.present<T_ARGUMENT_TYPE>(key);
    if (maybe_value) {
      out = *maybe_value;
    }
  } else {
    out = arg_parser.get<T_ARGUMENT_TYPE>(key);
  }
}

bool StringToBool(std::string_view name, std::string_view s) {
  std::string lowered(s);
  std::transform(s.begin(), s.end(), lowered.begin(), [](unsigned char c) { return std::tolower(c); });
  if (lowered == "true" or lowered == "1") {
    return true;
  }
  if (lowered == "false" or lowered == "0") {
    return false;
  }
  throw std::runtime_error(fmt::format(
      "[--{} {}] can't be converted to bool, must be '0', '1', 'true', or 'false' (case insensitive)", name, s));
}

template <typename T_ARGUMENT_TYPE> auto ArgParseAction(std::string_view name) {
  if constexpr (std::is_same_v<T_ARGUMENT_TYPE, bool>) {
    return [name = std::string(name)](const std::string &s) { return StringToBool(name, s); };
  } else if constexpr (std::is_floating_point_v<T_ARGUMENT_TYPE>) {
    // arg.template scan<'g', T_ARGUMENT_TYPE>();
    return argparse::details::parse_number<T_ARGUMENT_TYPE, argparse::details::chars_format::general>();
  } else if constexpr (std::is_arithmetic_v<T_ARGUMENT_TYPE>) {
    // arg.template scan<'i', T_ARGUMENT_TYPE>();
    return argparse::details::parse_number<T_ARGUMENT_TYPE>();
  } else if constexpr (std::is_same_v<T_ARGUMENT_TYPE, std::string>) {
    // identity
    return [](const std::string &s) { return s; };
  }
}

template <typename T_ARGUMENT_TYPE>
void ArgumentSetDefaultValue(argparse::Argument &arg, std::string_view name, const std::string &default_value) {
  arg.default_value(ArgParseAction<T_ARGUMENT_TYPE>(name)(default_value));
}

template <typename T_ARGUMENT_TYPE> void ArgumentTypeValidator(argparse::Argument &arg, std::string_view name) {
  arg.action(ArgParseAction<T_ARGUMENT_TYPE>(name));
}

} // namespace basis::arguments::types