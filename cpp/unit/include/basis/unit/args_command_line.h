#pragma once
/**
 * @file args_command_line.h
 *
 * Only contains CommandLineTypes, to avoid pulling in argparse to the main unit headers
 */
#include <map>
#include <string>
#include <tuple>
#include <variant>

namespace basis::unit {
/**
 * Wrapper to allow specifying argc+argv, key value pairs, or a vector of strings for arguments.
 */
using CommandLineTypes = std::variant<
  std::vector<std::pair<std::string, std::string>>, // key value
  std::vector<std::string>,
  std::pair<int, char const *const*>>; // argc+argv - NOTE: includes program name

}