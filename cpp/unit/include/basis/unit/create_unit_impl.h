#pragma once
/**
 * @file create_unit_impl.h
 *
 */
#include "args_template.h"
#include "create_unit.h"

namespace basis::unit {
template <typename T_UNIT>
basis::Unit *CreateUnit(const std::optional<std::string_view> &unit_name_override,
                        const basis::arguments::CommandLineTypes &command_line,
                        basis::CreateUnitLoggerInterface error_logger, const std::string_view unit_type_name) {
  auto args = T_UNIT::Args::ParseArgumentsVariant(command_line);

  if (!args.has_value()) {
    error_logger(fmt::format("Failed to launch {} ({}), bad arguments:\n{}",
                             unit_name_override.value_or(unit_type_name), unit_type_name, args.error())
                     .c_str());
    return nullptr;
  }

  auto maybe_templated_topics = basis::unit::RenderTemplatedTopics(*args, T_UNIT::all_templated_topics);
  if (!maybe_templated_topics) {
    error_logger(fmt::format("Failed to parse templates for {} ({}):\t\n{}",
                             unit_name_override.value_or(unit_type_name), unit_type_name,
                             maybe_templated_topics.error())
                     .c_str());
    return nullptr;
  }

  return new T_UNIT(*args, unit_name_override);
}
} // namespace basis::unit