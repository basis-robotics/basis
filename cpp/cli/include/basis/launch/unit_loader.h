#include <basis/unit.h>
#include <filesystem>
#include <memory>

/**
 * Creates a unit given a path to a shared object.
 * Will load the shared object, inspect it for required functionality, and return an initialized Unit.
 *
 * @param path the path to the shared object
 * @return std::unique_ptr<basis::Unit>
 */
std::unique_ptr<basis::Unit> CreateUnitWithLoader(const std::filesystem::path &path, std::string_view unit_name, const basis::unit::CommandLineTypes& command_line);
