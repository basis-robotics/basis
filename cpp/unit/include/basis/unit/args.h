#pragma once
/**
 * @file args.h
 *
 * Contains helpers to both work with generic argument declarations, as well as do argument parsing for units.
 */

#include <argparse/argparse.hpp>

#include <string>

#include "args_command_line.h"

namespace basis::unit {

/**
 * Base, allowing argument definitions to be stored in array<unique_ptr>, to avoid having to reach for tuple
 */
struct ArgumentMetadataBase {
  ArgumentMetadataBase(const std::string &name, const std::string &help_text, const std::string &type_name,
                       bool optional)
      : name(name), help_text(help_text), type_name(type_name), optional(optional) {}
  virtual ~ArgumentMetadataBase() = default;

  /**
   * Adds a single argument to an argparse::ArgumentParser
   */
  virtual void CreateArgparseArgument(argparse::ArgumentParser &parser_out) = 0;

  std::string name;
  std::string help_text;
  std::string type_name;
  bool optional = false;
};

/**
 * @brief Simple helper to convert <T, bool> to either T or std::optional<T>
 */
template <typename T, bool IS_OPTIONAL> struct ArgumentOptionalHelper {
  using type = T;
};

template <typename T> struct ArgumentOptionalHelper<T, true> {
  using type = std::optional<T>;
};

/**
 * A type aware container for metadata around a unit argument.
 *
 * @tparam T_ARGUMENT_TYPE the type we will cast this argument to
 */
template <typename T_ARGUMENT_TYPE> struct ArgumentMetadata : public ArgumentMetadataBase {
  ArgumentMetadata(const std::string &name, const std::string &help_text, const std::string &type_name, bool optional,
                   std::optional<T_ARGUMENT_TYPE> default_value = {})
      : ArgumentMetadataBase(name, help_text, type_name, optional), default_value(default_value) {}

  virtual void CreateArgparseArgument(argparse::ArgumentParser &parser) override {
    auto &arg = parser.add_argument("--" + ArgumentMetadataBase::name).help(ArgumentMetadataBase::help_text);

    // If we aren't optional, we're required
    if (!ArgumentMetadataBase::optional) {
      arg.required();
    }

    if constexpr (std::is_same_v<T_ARGUMENT_TYPE, bool>) {
      arg.action([&](const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lowered == "true" or lowered == "1") {
          return true;
        }
        if (lowered == "false" or lowered == "0") {
          return false;
        }
        throw std::runtime_error(
            fmt::format("[--{} {}] can't be converted to bool, must be '0', '1', 'true', or 'false' (case insensitive)",
                        ArgumentMetadataBase::name, value));
      });
    } else if constexpr (std::is_floating_point_v<T_ARGUMENT_TYPE>) {
      arg.template scan<'g', T_ARGUMENT_TYPE>();
    } else if constexpr (std::is_arithmetic_v<T_ARGUMENT_TYPE>) {
      arg.template scan<'i', T_ARGUMENT_TYPE>();
    }
    if (default_value) {
      arg.default_value(*default_value);
    }
  }

  // The default value to use, if any
  std::optional<T_ARGUMENT_TYPE> default_value;
};

/**
 * A base container for all arguments for a unit
 *
 * @tparam T_DERIVED the derived type, used for CRTP. Will have a typed member for each argument.
 */
template <typename T_DERIVED> struct UnitArguments {
  /**
   * Create an Argument Parser for use with the associated unit
   *
   * @return std::unique_ptr<argparse::ArgumentParser> (never null - pointer due to move construction disabled with
   * ArgumentParser)
   */
  static std::unique_ptr<argparse::ArgumentParser> CreateArgumentParser() {
    auto parser = std::make_unique<argparse::ArgumentParser>();

    for (auto &arg : T_DERIVED::argument_metadatas) {
      arg->CreateArgparseArgument(*parser);
    }

    return parser;
  }

  /**
   * Creates a T_DERIVED with each field filled out from the command line input.
   *
   * @param command_line The arguments to parse
   * @return nonstd::expected<T_DERIVED, std::string> either a filled in T_DERIVED or an error string
   */
  static nonstd::expected<T_DERIVED, std::string> ParseArgumentsVariant(const CommandLineTypes &command_line) {
    return std::visit([](auto &&command_line) { return ParseArguments(command_line); }, command_line);
  }

  /**
   * Specialized version of ParseArgumentsVariant handling key value pairs
   * @note: will add a fake program name
   */
  static nonstd::expected<T_DERIVED, std::string>
  ParseArguments(const std::vector<std::pair<std::string, std::string>> &argument_pairs) {
    std::vector<std::string> command_line;
    command_line.reserve(argument_pairs.size() * 2 + 1);

    // Add fake program name
    command_line.push_back("");
    for (auto &[k, v] : argument_pairs) {
      // Add --k v
      command_line.push_back("--" + k);
      command_line.push_back(v);
    }

    return ParseArgumentsInternal(command_line);
  }

  /**
   * Specialized version of ParseArgumentsVariant handling argc+argv
   * @note: will not add a fake program name - it's required to be supplied.
   */
  static nonstd::expected<T_DERIVED, std::string> ParseArguments(std::pair<int, const char *const *> argc_argv) {
    // In this case, it's assumed you have a raw command line from main() - no need to add a fake program name
    return ParseArguments(argc_argv.first, argc_argv.second);
  }

  /**
   * Specialized version of ParseArgumentsVariant handling argc+argv
   * @note: will not add a fake program name - it's required to be supplied.
   */
  static nonstd::expected<T_DERIVED, std::string> ParseArguments(int argc, const char *const *argv) {
    // In this case, it's assumed you have a raw command line from main() - no need to add a
    // https://github.com/p-ranav/argparse/blob/v3.1/include/argparse/argparse.hpp#L1868
    return ParseArgumentsInternal(std::vector<std::string>{argv, argv + argc});
  }

  /**
   * Specialized version of ParseArgumentsVariant handling a vector of strings
   * @note: will add a fake program name
   */
  static nonstd::expected<T_DERIVED, std::string> ParseArguments(const std::vector<std::string> &command_line) {
    std::vector<std::string> command_line_with_program;
    command_line_with_program.reserve(command_line.size() + 1);
    // Handle "program name" argument
    command_line_with_program.push_back("");
    command_line_with_program.insert(command_line_with_program.end(), command_line.begin(), command_line.end());
    return ParseArgumentsInternal(command_line_with_program);
  }

private:
  /**
   * Create an argument parser and parse the command line
   * @param command_line
   * @return nonstd::expected<T_DERIVED, std::string> either a filled in T_DERIVED or an error string
   */
  static nonstd::expected<T_DERIVED, std::string> ParseArgumentsInternal(const std::vector<std::string> &command_line) {
    T_DERIVED out;

    auto parser = CreateArgumentParser();

    try {
      parser->parse_args(command_line);
    } catch (const std::exception &err) {
      // argparse uses exceptions for error handling - handle it and convert to an error
      return nonstd::make_unexpected(err.what());
    }

    // Ask the derived struct to handle each field
    // This is done via code-generation
    /// @todo: this can likely be done via metadata, at the cost of more std::tuple abuse
    out.HandleParsedArgs(*parser);

    return out;
  }
};

} // namespace basis::unit
