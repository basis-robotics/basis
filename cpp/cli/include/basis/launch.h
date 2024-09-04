#include <string_view>
#include <optional>
#include <filesystem>
#include <vector>
#include <basis/core/logging/macros.h>

DEFINE_AUTO_LOGGER_NS(basis::launch)

namespace basis::launch {

/**
 * Search for a unit in well known directories given a unit name
 * @param unit_name
 * @return std::optional<std::filesystem::path>
 */
std::optional<std::filesystem::path> FindUnit(std::string_view unit_name);

/**
 * Launch a set of processes, given a path to a yaml.
 * @param yaml_path the yaml to launch
 * @param args command line args given to the launch
 * @param process_name_filter If empty - will fork once per process in the yaml. If not empty, will load each unit in
 * the yaml for the requested process.
 */
void LaunchYamlPath(std::string_view yaml_path, const std::vector<std::string> &args,
                    std::string process_name_filter = "", bool sim = false);


}