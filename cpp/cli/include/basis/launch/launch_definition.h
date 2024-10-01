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
};

struct ProcessDefinition {
  std::unordered_map<std::string, UnitDefinition> units;
};

struct RecordingSettings {
  bool async = true;
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
};

std::optional<LaunchDefinition> ParseTemplatedLaunchDefinitionYAMLPath(std::filesystem::path yaml_path,
                                                                       const LaunchContext &context);
std::optional<LaunchDefinition> ParseTemplatedLaunchDefinitionYAMLContents(std::string_view yaml_contents,
                                                                           const LaunchContext &context);

LaunchDefinition ParseLaunchDefinitionYAML(const YAML::Node &yaml);

} // namespace basis::launch