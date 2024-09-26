#pragma once
/**
 * @file launch_definition.h
 *
 * File responsible for converting from a yaml file to C++ launch definitions
 */
#include <filesystem>
#include <string>
#include <optional>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include <basis/recorder/glob.h>

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
LaunchDefinition ParseLaunchDefinitionYAML(const YAML::Node &yaml);