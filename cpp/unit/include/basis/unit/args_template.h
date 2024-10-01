#pragma once
/**
 * @file args_template.h
 *
 */
#include <basis/arguments.h>
#include <nonstd/expected.hpp>
#include <unordered_map>
#include <vector>

namespace basis::unit {
/**
 * Converts from from topics like {{args.camera_name}}/rgb -> /webcam/rgb using inja
 * @param args key value pairs of the arguments used for context in the template engine
 * @param topics the list of possibly templated topics to convert
 * @return nonstd::expected<std::unordered_map<std::string, std::string>, std::string> the rendered topics
 */
nonstd::expected<std::unordered_map<std::string, std::string>, std::string>
RenderTemplatedTopics(const basis::arguments::ArgumentsBase &args, const std::vector<std::string> &topics);

} // namespace basis::unit