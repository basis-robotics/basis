/**
 * @file launch_definition.h
 *
 * File responsible for converting from a yaml file to C++ launch definitions
 */
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

struct UnitDefinition {
    std::string unit_type;
};

struct ProcessDefinition {
    std::unordered_map<std::string, UnitDefinition> units;
};

struct LaunchDefinition {
    std::unordered_map<std::string, ProcessDefinition> processes;
};

LaunchDefinition ParseLaunchDefinitionYAML(YAML::Node yaml) {
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
    return launch;
}