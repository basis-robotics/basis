#include <string>
#include "args_command_line.h"
namespace basis {
    class Unit;
    using CreateUnitLoggerInterface = void(*)(const char*);
}

extern "C" {
    // TODO: add logger arg
    basis::Unit* CreateUnit(const std::optional<std::string_view>& unit_name_override, const basis::unit::CommandLineTypes& command_line, basis::CreateUnitLoggerInterface error_logger);
}