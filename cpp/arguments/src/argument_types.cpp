#include <basis/arguments/argument_types.h>
#include <nlohmann/json.hpp>

namespace basis::arguments::types {
using namespace std;
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

} // namespace basis::arguments::types