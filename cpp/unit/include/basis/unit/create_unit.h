#pragma once
/**
 * @file create_unit.h
 *
 * Contains the CreateUnit definition. Please use this to avoid drift between declaration and usage of CreateUnit.
 * Functions linked as extern "C" can't do interface type checking when used via dlsym!
 */
#include <basis/arguments/command_line.h>
#include <optional>
#include <string_view>

namespace basis {
class Unit;
/**
 * Logger interface mainly for error logs. Dead simple, only accepts char*.
 */
using CreateUnitLoggerInterface = void (*)(const char *);
} // namespace basis

extern "C" {
/**
 * Forward declaration of CreateUnit - declared once in each unit library to provide an easy interface to create the
 * contained unit without prior type knowledge. Basically - the entrypoint into a unit "plugin"
 *
 * @param unit_name_override Optionally override the name of the unit.
 * @param command_line The arguments given to this unit, can be one of several types - see command_line.h
 * @param error_logger The logger to use when there's an error creating the unit (we don't assume that basis logging
 * system is initialized).
 * @return basis::Unit* The created unit, or nullptr if there was an error.
 */
basis::Unit *CreateUnit(const std::optional<std::string_view> &unit_name_override,
                        const basis::arguments::CommandLineTypes &command_line,
                        basis::CreateUnitLoggerInterface error_logger);
}