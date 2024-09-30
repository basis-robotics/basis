#pragma once
/**
 * @file args.h
 *
 * Contains helpers to both work with generic argument declarations, as well as do argument parsing for units.
 */

#include <span>
#include <string>
#include <unordered_map>

#include <argparse/argparse.hpp>
#include <nonstd/expected.hpp>
#include <spdlog/fmt/fmt.h>

#include "arguments/command_line.h"
// TODO: we can factor this out into a cpp file, with some work
#include <nlohmann/json.hpp>

namespace basis::arguments {

// TODO: move to separate header
namespace types {
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

#define DECLARE_ARGUMENT_TYPE(type)                                                                                    \
  {.type_name = #type, .validator = &ArgumentTypeValidator<type>, .to_json = &ArgumentToJson<type>}

#define X_TYPE(type) {#type, DECLARE_ARGUMENT_TYPE(type)},
inline const std::unordered_map<std::string, TypeMetadata> allowed_argument_types = {X_ALLOWED_ARGUMENT_TYPES};

template <typename T> struct TypeToTypeMetadata {};

#undef X_TYPE
#define X_TYPE(type)                                                                                                   \
  template <> struct TypeToTypeMetadata<type> {                                                                        \
    const inline static TypeMetadata metadata DECLARE_ARGUMENT_TYPE(type);                                             \
  };

X_ALLOWED_ARGUMENT_TYPES

#undef X_TYPE
#undef X_ALLOWED_ARGUMENT_TYPES

} // namespace types
/**
 * Base, allowing argument definitions to be stored in array<unique_ptr>, to avoid having to reach for tuple
 */
struct ArgumentMetadataBase {
  ArgumentMetadataBase(const std::string &name, const std::string &help_text, const types::TypeMetadata &type_metadata,
                       bool optional)
      : name(name), help_text(help_text), type_metadata(type_metadata), optional(optional) {}
  virtual ~ArgumentMetadataBase() = default;

  /**
   * Adds a single argument to an argparse::ArgumentParser
   */
  void CreateArgparseArgument(argparse::ArgumentParser &parser) {
    auto &arg = parser.add_argument("--" + name).help(help_text);

    // If we aren't optional, we're required
    if (!optional) {
      arg.required();
    }

    type_metadata.validator(arg, name);

    SetDefaultValue(arg);
  }

  virtual void SetDefaultValue(argparse::Argument &argument) = 0;

  std::string name;
  std::string help_text;
  const types::TypeMetadata &type_metadata;
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
template <typename T_ARGUMENT_TYPE> struct TypedArgumentMetadata : public ArgumentMetadataBase {
  TypedArgumentMetadata(const std::string &name, const std::string &help_text, bool optional,
                        std::optional<T_ARGUMENT_TYPE> default_value = {})
      : ArgumentMetadataBase(name, help_text, types::TypeToTypeMetadata<T_ARGUMENT_TYPE>::metadata, optional),
        default_value(default_value) {}

  virtual void SetDefaultValue(argparse::Argument &argument) override {
    if (default_value) {
      argument.default_value(*default_value);
    }
  }

  // The default value to use, if any
  std::optional<T_ARGUMENT_TYPE> default_value;
};

struct StringArgumentMetadata : public ArgumentMetadataBase {
protected:
  StringArgumentMetadata(const std::string &name, const std::string &help_text,
                         const types::TypeMetadata &type_metadata, bool optional,
                         std::optional<std::string> default_value = {})
      : ArgumentMetadataBase(name, help_text, type_metadata, optional), default_value(default_value) {}

public:
  static std::unique_ptr<StringArgumentMetadata> Create(const std::string &name, const std::string &help_text,
                                                        const std::string &type_name, bool optional,
                                                        std::optional<std::string> default_value = {}) {
    auto it = types::allowed_argument_types.find(type_name);
    if (it == types::allowed_argument_types.end()) {
      return nullptr;
    }
    return std::unique_ptr<StringArgumentMetadata>(
        new StringArgumentMetadata(name, help_text, it->second, optional, default_value));
  }

  virtual void SetDefaultValue(argparse::Argument &argument) override {
    if (default_value) {
      argument.default_value(*default_value);
    }
  }

  // The default value to use, if any
  std::optional<std::string> default_value;
};

struct ArgumentsBase {
  virtual ~ArgumentsBase() = default;
  const std::unordered_map<std::string, std::string> &GetArgumentMapping() const { return args_map; }

  static std::unique_ptr<argparse::ArgumentParser>
  CreateArgumentParser(const std::span<std::unique_ptr<ArgumentMetadataBase>> argument_metadatas) {
    auto parser = std::make_unique<argparse::ArgumentParser>();

    for (auto &arg : argument_metadatas) {
      arg->CreateArgparseArgument(*parser);
    }

    return parser;
  }

protected:
  std::unordered_map<std::string, std::string> args_map;
};

/**
 * A base container for all arguments for a unit
 *
 * @tparam T_DERIVED the derived type, used for CRTP. Will have a typed member for each argument.
 */
template <typename T_DERIVED> struct UnitArguments : public ArgumentsBase {
  /**
   * Create an Argument Parser for use with the associated unit
   *
   * @return std::unique_ptr<argparse::ArgumentParser> (never null - pointer due to move construction disabled with
   * ArgumentParser)
   */
  static std::unique_ptr<argparse::ArgumentParser> CreateArgumentParser() {
    return ArgumentsBase::CreateArgumentParser(T_DERIVED::argument_metadatas);
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

} // namespace basis::arguments
