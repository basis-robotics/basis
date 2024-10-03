#include "argparse/argparse.hpp"
#include "basis/core/logging/macros.h"
#include "spdlog/fmt/bundled/format.h"
#include "yaml-cpp/eventhandler.h"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/parser.h"
#include <basis/launch/launch_definition.h>

#include <basis/launch.h>

#include <filesystem>
#include <fstream>

#include <inja/inja.hpp>

#include <basis/arguments.h>
#include <string_view>

namespace basis::launch {
std::string LaunchDefinitionToDebugString(const LaunchDefinition &launch) {
  std::vector<std::string> process_strs;
  for (auto &[process_name, process] : launch.processes) {
    process_strs.emplace_back(basis::launch::ProcessDefinitionToDebugString(process_name, process));
  }
  return fmt::format("{}", fmt::join(process_strs, "\n"));
}
std::string ProcessDefinitionToDebugString(std::string_view process_name, const ProcessDefinition &process) {
  std::vector<std::string> unit_cmds;
  for (const auto &[unit_name, unit] : process.units) {
    std::vector<std::string> args;
    args.reserve(unit.args.size());
    for (auto &p : unit.args) {
      args.emplace_back(fmt::format("--{} {}", p.first, p.second));
    }
    unit_cmds.emplace_back(fmt::format("  {}: {} {}", unit_name, unit.unit_type, fmt::join(args, " ")));
  }
  return fmt::format("process \"{}\" with {} units\n{}", process_name, process.units.size(),
                     fmt::join(unit_cmds, "\n"));
}
constexpr int MAX_LAUNCH_INCLUDE_DEPTH = 32;

[[nodiscard]] RecordingSettings ParseRecordingSettingsYAML(const YAML::Node &yaml) {
  RecordingSettings settings;

  if (yaml["directory"]) {
    settings.directory = yaml["directory"].as<std::string>();
  }

  for (const auto &pattern_yaml : yaml["topics"]) {
    settings.patterns.emplace_back(glob::GlobToRegex(pattern_yaml.as<std::string>()));
  }

  if (yaml["async"]) {
    settings.async = yaml["async"].as<bool>();
  }

  if (yaml["name"]) {
    settings.name = yaml["name"].as<std::string>();
  }

  return settings;
}

[[nodiscard]] std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLPath(const std::filesystem::path &yaml_path,
                                       const basis::arguments::CommandLineTypes &command_line, size_t recursion_depth) {

  std::ifstream file(yaml_path.string(), std::ios::binary | std::ios::ate);
  if (file.fail()) {
    // TODO: maybe use ifstream::exceptions
    BASIS_LOG_FATAL("Launch file {} isn't openable.", yaml_path.string());
    return {};
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (file.read(buffer.data(), size)) {
    return ParseTemplatedLaunchDefinitionYAMLContents(
        {buffer.data(), (size_t)size}, command_line,
        CurrentLaunchParseState(std::filesystem::canonical(yaml_path), recursion_depth));
  }
  BASIS_LOG_FATAL("Failed to load file {}", yaml_path.string());
  return {};
}

namespace {
using namespace YAML;
class DocumentFinder : public YAML::EventHandler {
public:
  virtual ~DocumentFinder() = default;

  virtual void OnDocumentStart(const Mark &mark) { document_start = mark.pos; }
  virtual void OnDocumentEnd() {}

  virtual void OnNull([[maybe_unused]] const Mark &mark, [[maybe_unused]] anchor_t anchor) {};
  virtual void OnAlias([[maybe_unused]] const Mark &mark, [[maybe_unused]] anchor_t anchor) {};
  virtual void OnScalar([[maybe_unused]] const Mark &mark, [[maybe_unused]] const std::string &tag,
                        [[maybe_unused]] anchor_t anchor, [[maybe_unused]] const std::string &value) {};

  virtual void OnSequenceStart([[maybe_unused]] const Mark &mark, [[maybe_unused]] const std::string &tag,
                               [[maybe_unused]] anchor_t anchor, [[maybe_unused]] EmitterStyle::value style) {};
  virtual void OnSequenceEnd() {};

  virtual void OnMapStart([[maybe_unused]] const Mark &mark, [[maybe_unused]] const std::string &tag,
                          [[maybe_unused]] anchor_t anchor, [[maybe_unused]] EmitterStyle::value style) {};
  virtual void OnMapEnd() {};

  virtual void OnAnchor([[maybe_unused]] const Mark & /*mark*/, [[maybe_unused]] const std::string & /*anchor_name*/) {}
  size_t document_start = 0;
};
} // namespace
[[nodiscard]] std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLContents(std::string_view yaml_contents, const nlohmann::json &template_data,
                                           const CurrentLaunchParseState &current_parse_state);
[[nodiscard]] std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLContents([[maybe_unused]] std::string_view yaml_contents,
                                           const basis::arguments::CommandLineTypes &command_line,
                                           const CurrentLaunchParseState &current_parse_state) {

  // Now separate out arguments from templated content
  // TODO cpp 23 https://en.cppreference.com/w/cpp/io/basic_ispanstream to avoid copy
  std::string yaml_contents_copy = std::string(yaml_contents);
  std::stringstream stream(yaml_contents_copy);
  auto yaml_parser = YAML::Parser(stream);

  DocumentFinder document_finder;
  // Skip the first document, which must be args, if multiple exist
  yaml_parser.HandleNextDocument(document_finder);

  std::unique_ptr<argparse::ArgumentParser> argparser;

  std::vector<std::unique_ptr<arguments::ArgumentMetadataBase>> argument_metadata;

  const size_t first_doc_marker = document_finder.document_start;
  try {
    yaml_parser.HandleNextDocument(document_finder);
  } catch (std::exception &) {
    // Due to templating causing invalid yaml, this might not succeed - but we will still get a document start marker
  }

  // Look for the content document
  if (first_doc_marker != document_finder.document_start) {
    // It exists
    const size_t template_content_start = document_finder.document_start;

    // The first document in the launch file with two documents cannot contain inja declarations, load it
    YAML::Node first_document = YAML::Load(yaml_contents_copy);

    if (YAML::Node args_yaml = first_document["args"]) {
      // Now, parse the args
      for (const auto &arg_name_yaml : args_yaml) {
        const auto &arg_name = arg_name_yaml.first.as<std::string>();
        const auto &arg_yaml = arg_name_yaml.second;
        // Help is optional
        std::string help;
        if (arg_yaml["help"]) {
          help = arg_yaml["help"].as<std::string>();
        }
        // default value is optional
        std::optional<std::string> default_value;
        if (arg_yaml["default"]) {
          default_value = arg_yaml["default"].as<std::string>();
        }

        bool optional = arg_yaml["optional"] && arg_yaml["optional"].as<bool>();
        if (optional && default_value) {
          BASIS_LOG_FATAL("argument {} cannot have both optional and a default value", arg_name);
        }
        auto argument = arguments::StringArgumentMetadata::Create(arg_name, help, arg_yaml["type"].as<std::string>(),
                                                                  optional, default_value);
        if (!argument) {
          BASIS_LOG_FATAL("Failed to parse args[{}]", arg_name);
          return {};
        }
        argument_metadata.push_back(std::move(argument));
      }
      argparser = arguments::ArgumentsBase::CreateArgumentParser(argument_metadata, false);
    }
    // Adjust the pointer to the start of templated content
    yaml_contents = yaml_contents.substr(template_content_start);
  }

  // If we specified no arguments, create an empty argparser so that we get consistent error messages
  if (!argparser) {
    argparser = std::make_unique<argparse::ArgumentParser>();
  }

  try {
    std::vector<std::string> argparse_args = basis::arguments::CommandLineToArgparseArgs(
        "basis launch " + current_parse_state.current_file_path.string(), command_line);
    argparser->parse_args(argparse_args);
  } catch (const std::exception &err) {
    // argparse uses exceptions for error handling - handle it and convert to an error
    BASIS_LOG_FATAL("{}", err.what());

    BASIS_LOG_FATAL("{}", argparser->help().str());
    return {};
  }
  nlohmann::json template_data;

  for (const auto &metadata : argument_metadata) {
    metadata->type_metadata.to_json(*argparser, metadata->name, metadata->optional,
                                    template_data["args"][metadata->name]);
  }

  return ParseTemplatedLaunchDefinitionYAMLContents(yaml_contents, template_data, current_parse_state);
}

[[nodiscard]] std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLContents(std::string_view yaml_contents, const nlohmann::json &template_data,
                                           const CurrentLaunchParseState &current_parse_state) {
  inja::Environment inja_env;
  inja_env.set_throw_at_missing_includes(true);

  std::string rendered_contents;
  try {
    rendered_contents = inja_env.render(yaml_contents, template_data);
  } catch (const std::exception &e) {
    BASIS_LOG_FATAL("Failed to render templated launch file: {}", e.what());
    return {};
  }
  try {
    // TODO: will need to show the parsed jinja on error
    auto node = YAML::Load(rendered_contents);
    if (node["args"]) {
      // This is always an error - we want clear separation between templated and non-templated data
      BASIS_LOG_FATAL("args detected inline with launch content, required to be separated by document marker '---'");
      return {};
    }
    return ParseLaunchDefinitionYAML(node, current_parse_state);

  } catch (const std::exception &e) {
    BASIS_LOG_FATAL("Failed to parse launch file: {}", e.what());
    return {};
  }
}

[[nodiscard]] UnitDefinition ParseUnitDefinitionYAML(std::string_view unit_name, const YAML::Node &unit_yaml) {
  UnitDefinition unit;

  if (unit_yaml["unit"]) {
    // If we specify a unit type, use that
    unit.unit_type = unit_yaml["unit"].as<std::string>();
  } else {
    // By default, the unit name is the unit type
    unit.unit_type = unit_name;
  }

  if (unit_yaml["args"]) {
    for (const auto &kv : unit_yaml["args"]) {
      unit.args.emplace_back(std::pair{kv.first.as<std::string>(), kv.second.as<std::string>()});
    }
  }

  return unit;
}

[[nodiscard]] std::optional<LaunchDefinition> Include(const CurrentLaunchParseState &current_parse_state,
                                                      [[maybe_unused]] std::string_view definition_name,
                                                      [[maybe_unused]] const YAML::Node &args) {
  if (current_parse_state.launch_file_recursion_depth >= MAX_LAUNCH_INCLUDE_DEPTH) {
    BASIS_LOG_FATAL("Hit maximum recursion depth {} including {}", MAX_LAUNCH_INCLUDE_DEPTH, definition_name);
    return {};
  }

  // First, find the definition
  std::filesystem::path definition_path =
      std::filesystem::canonical(current_parse_state.include_search_directory / definition_name);

  std::vector<std::pair<std::string, std::string>> launch_args;
  launch_args.reserve(args.size());

  for (const auto &p : args) {
    launch_args.emplace_back(p.first.as<std::string>(), p.second.as<std::string>());
  }

  std::optional<LaunchDefinition> launch_definition = ParseTemplatedLaunchDefinitionYAMLPath(
      definition_path, launch_args, current_parse_state.launch_file_recursion_depth + 1);
  return launch_definition;
}

[[nodiscard]] bool MoveUnitToProcess(ProcessDefinition *process, const std::string &unit_name,
                                     std::string_view process_name, UnitDefinition &&unit) {
  // Create the new unit name
  auto it = process->units.find(unit_name);

  if (it != process->units.end()) {
    BASIS_LOG_FATAL("{}: Process {} contains unit named {} from {}, unit names must be "
                    "unique within a process",
                    unit.source_file, process_name, unit_name, it->second.source_file);
    return false;
  }
  process->units.emplace(unit_name, std::move(unit));
  return true;
}

[[nodiscard]] bool ParseIncludeTag(const YAML::Node &parent_include_yaml, LaunchDefinition &launch,
                                   ProcessDefinition *parent_process,
                                   const CurrentLaunchParseState &current_parse_state, const char *parent_process_name,
                                   const std::string &parent_group_name) {
  if (parent_include_yaml) {
    for (const auto &include_item : parent_include_yaml) {
      std::string include_name;
      const YAML::Node *include_yaml = nullptr;

      if (parent_include_yaml.IsMap()) {
        /*
  # The default syntax that most will reach for
  include:
    foxglove.launch.yaml: {}
    perception.launch.yaml: {}
         */
        include_name = include_item.first.as<std::string>();
        include_yaml = &include_item.second;
      } else if (parent_include_yaml.IsSequence()) {
        /*
        # Allowing including the same launch file twice
  include:
  - camera.launch.yaml:
      camera: front
  - camera.launch.yaml:
      camera: back
  */
        for (const auto &p : include_item) {
          if (include_yaml) {
            BASIS_LOG_FATAL("{}: A yaml has mixed map and sequence items for includes.",
                            current_parse_state.current_file_path.string());
            return false;
          }
          include_name = p.first.as<std::string>();
          include_yaml = &p.second;
        }
        if (!include_yaml || !*include_yaml) {
          BASIS_LOG_FATAL("{}: include sequence item was empty", current_parse_state.current_file_path.string());
          return false;
        }
      } else if (parent_include_yaml.IsScalar()) {
      }
      if (!include_yaml) {
        BASIS_LOG_FATAL("{}: include tag was neither map nor list", current_parse_state.current_file_path.string());

        return false;
      }

      auto maybe_included = Include(current_parse_state, include_name, *include_yaml);
      if (!maybe_included) {
        // Failed to include, this is an error
        return false;
      }

      // merge launch files
      for (auto &[included_process_name, included_process] : maybe_included->processes) {
        // root process, merge with current process
        if (included_process_name == "/") {
          for (auto &[unit_name, unit] : included_process.units) {
            // Create the new unit name
            std::string full_unit_name =
                (std::filesystem::path(parent_group_name) / ("./" + unit_name)).lexically_normal();
            std::cout << parent_group_name << "+" << unit_name << "=" << full_unit_name << std::endl;
            if (!MoveUnitToProcess(parent_process, full_unit_name, parent_process_name, std::move(unit))) {
              return false;
            }
          }
        } else {
          std::string full_process_name =
              (std::filesystem::path(parent_group_name) / ("./" + included_process_name)).lexically_normal();
          auto it = launch.processes.find(full_process_name);
          if (it != launch.processes.end()) {
            BASIS_LOG_FATAL(
                "{}: Launch definition already contains a process named {} from {}, process group names must be "
                "unique within a launch",
                current_parse_state.current_file_path.string(), full_process_name, it->second.source_file);
            return false;
          }
          launch.processes.emplace(std::move(full_process_name), std::move(included_process));
        }
      }
    }
  }
  return true;
}

[[nodiscard]] bool ParseGroupDefinitionYAML(const YAML::Node &group_yaml, LaunchDefinition &launch,
                                            const CurrentLaunchParseState &current_parse_state,
                                            const char *parent_process_name = nullptr,
                                            ProcessDefinition *process = nullptr, const std::string &group_name = "/") {
  // todo: validate group names
  if (!process || (group_yaml["process"] && group_yaml["process"].as<bool>())) {
    auto it = launch.processes.find(group_name);
    if (it != launch.processes.end()) {
      BASIS_LOG_FATAL("{}: Launch definition already contains a process named {} from {}, process group names must be "
                      "unique within a launch",
                      current_parse_state.current_file_path.string(), group_name, it->second.source_file);
      return false;
    }
    process = &launch.processes[group_name];
    parent_process_name = group_name.c_str();
    process->source_file = current_parse_state.current_file_path;
  }

  for (const auto &unit_name_yaml : group_yaml["units"]) {
    const auto &unit_name = unit_name_yaml.first.as<std::string>();
    // TODO: validate unit names
    const auto &unit_yaml = unit_name_yaml.second;
    UnitDefinition unit = ParseUnitDefinitionYAML(unit_name, unit_yaml);
    unit.source_file = current_parse_state.current_file_path;
    std::string full_unit_name = std::filesystem::path(group_name) / unit_name;

    if (!MoveUnitToProcess(process, full_unit_name, parent_process_name, std::move(unit))) {
      return false;
    }
  }

  for (const auto &p : group_yaml["groups"]) {
    const auto &inner_group_name = p.first.as<std::string>();

    if (!ParseGroupDefinitionYAML(p.second, launch, current_parse_state, parent_process_name, process,
                                  std::filesystem::path(group_name) / inner_group_name)) {
      return false;
    }
  }

  if (!ParseIncludeTag(group_yaml["include"], launch, process, current_parse_state, parent_process_name, group_name)) {
    return false;
  }

  return true;
}

[[nodiscard]] std::optional<LaunchDefinition>
ParseLaunchDefinitionYAML(const YAML::Node &yaml, const CurrentLaunchParseState &current_parse_state) {
  LaunchDefinition launch;

  if (!ParseGroupDefinitionYAML(yaml, launch, current_parse_state)) {
    return {};
  }

  if (yaml["recording"]) {
    launch.recording_settings = ParseRecordingSettingsYAML(yaml["recording"]);
  }
  return launch;
}

} // namespace basis::launch