#include <filesystem>
#include <memory>
#include <basis/unit.h>

/**
 * Creates a unit given a path to a shared object.
 * Will load the shared object, inspect it for required functionality, and return an initialized Unit.
 *
 * @param path the path to the shared object
 * @return std::unique_ptr<basis::Unit>
 */
std::unique_ptr<basis::Unit> CreateUnit([[maybe_unused]] const std::filesystem::path& path);
