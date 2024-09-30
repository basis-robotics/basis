#include "basis/launch/launch_definition.h"
#include <basis/core/logging/macros.h>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

DEFINE_AUTO_LOGGER_NS(basis::launch)

namespace basis::launch {

/**
 * Search for a unit in well known directories given a unit name
 * @param unit_name
 * @return std::optional<std::filesystem::path>
 */
std::optional<std::filesystem::path> FindUnit(std::string_view unit_name);

/**
 * Launch a set of processes, given a definition
 */
void LaunchYamlDefinition(const LaunchDefinition &launch, const LaunchContext &context);

} // namespace basis::launch