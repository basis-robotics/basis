#include <filesystem>
#include <memory>
#include <basis/unit.h>

std::unique_ptr<basis::Unit> CreateUnit([[maybe_unused]] const std::filesystem::path& path);
