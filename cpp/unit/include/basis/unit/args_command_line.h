#pragma once
#include <map>
#include <string>
#include <tuple>
#include <variant>

namespace basis::unit {
using CommandLineTypes = std::variant<
  std::vector<std::pair<std::string, std::string>>,
  std::vector<std::string>,
  std::pair<int, char const *const*>>;
}