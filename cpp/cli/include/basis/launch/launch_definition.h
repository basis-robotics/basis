#pragma once
/**
 * @file launch_definition.h
 *
 * File responsible for converting from a yaml file to C++ launch definitions
 */
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include <basis/arguments/command_line.h>

#include <basis/recorder/glob.h>

namespace basis::launch {

struct UnitDefinition {
  std::string unit_type;
  std::vector<std::pair<std::string, std::string>> args;
  std::string source_file;
};

struct ProcessDefinition {
  std::unordered_map<std::string, UnitDefinition> units;
  std::string source_file;
};

struct RecordingSettings {
  bool async = true;
  std::string name = "basis";
  std::vector<std::regex> patterns;
  std::filesystem::path directory;
};

struct LaunchDefinition {
  std::optional<RecordingSettings> recording_settings;
  std::unordered_map<std::string, ProcessDefinition> processes;
};

RecordingSettings ParseRecordingSettingsYAML(const YAML::Node &yaml);

struct LaunchContext {
  bool sim = false;
  std::string process_filter;
  std::vector<std::string> all_args;
  std::vector<std::string> launch_args;
  std::filesystem::path search_directory;
};

struct CurrentLaunchParseState {
  explicit CurrentLaunchParseState(const std::filesystem::path &current_file_path, size_t depth)
      : current_file_path(current_file_path), include_search_directory(current_file_path.parent_path()),
        launch_file_recursion_depth(depth) {}
  std::filesystem::path current_file_path;
  std::filesystem::path include_search_directory;
  size_t launch_file_recursion_depth;
};
// TODO: use common command line type, dummy
[[nodiscard]] std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLPath(const std::filesystem::path &yaml_path,
                                       const std::vector<std::string> &launch_args, size_t recursion_depth = 0);
[[nodiscard]] std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLContents(std::string_view yaml_contents, const std::vector<std::string> &launch_args,
                                           const CurrentLaunchParseState &current_parse_state);

[[nodiscard]] std::optional<LaunchDefinition>
ParseLaunchDefinitionYAML(const YAML::Node &yaml, const CurrentLaunchParseState &current_parse_state);

} // namespace basis::launch