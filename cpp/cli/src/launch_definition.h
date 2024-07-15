/**
 * @file launch_definition.h
 *
 * File responsible for converting from a yaml file to C++ launch definitions
 */
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include <basis/recorder/glob.h>

struct UnitDefinition {
    std::string unit_type;
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

RecordingSettings ParseRecordingSettingsYAML(const YAML::Node& yaml) {
    RecordingSettings settings;

    if(yaml["directory"]) {
        settings.directory = yaml["directory"].as<std::string>();
    }

    for(const auto& pattern_yaml : yaml["topics"]) {
        settings.patterns.emplace_back(glob::GlobToRegex(pattern_yaml.as<std::string>()));
    }

    if(yaml["async"]) {
        settings.async = yaml["async"].as<bool>();
    }

    return settings;
}

LaunchDefinition ParseLaunchDefinitionYAML(const YAML::Node& yaml) {
    LaunchDefinition launch;

    for(const auto& process_yaml : yaml["processes"]) {
        ProcessDefinition process;
        for(const auto& unit_name_yaml : process_yaml.second["units"]) {
            const auto& unit_name = unit_name_yaml.first.as<std::string>();
            const auto& unit_yaml = unit_name_yaml.second;
            UnitDefinition unit;

            if(unit_yaml["unit"]) {
                // If we specify a unit type, use that
                unit.unit_type = unit_yaml["unit"].as<std::string>();
            } else {
                // By default, the unit name is the unit type
                unit.unit_type = unit_name;
            }
            process.units.emplace(unit_name, std::move(unit));
        }
        launch.processes.emplace(process_yaml.first.as<std::string>(), std::move(process));
    }
    if(yaml["recording"]) {
        launch.recording_settings = ParseRecordingSettingsYAML(yaml["recording"]);
    }
    return launch;
}