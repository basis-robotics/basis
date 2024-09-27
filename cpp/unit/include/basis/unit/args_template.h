#pragma once
#include "basis/unit/args.h"
#include <nonstd/expected.hpp>
#include <unordered_map>
#include <vector>

namespace basis::unit {

nonstd::expected<std::unordered_map<std::string, std::string>, std::string>
ParseTemplatedTopics(const UnitArgumentsBase &args, const std::vector<std::string> &topics);

} // namespace basis::unit