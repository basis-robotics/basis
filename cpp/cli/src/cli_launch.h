#pragma once
#include "argparse/argparse.hpp"
#include "cli_subcommand.h"
#include <basis/cli_logger.h>
#include <basis/launch.h>
#include <basis/launch/launch_definition.h>

namespace basis::cli {
class LaunchCommand : public CLISubcommand {
public:
  LaunchCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("launch", parent_parser) {
    parser.add_description("launch a yaml");
    parser.add_argument("--process").help("The process to launch inside the yaml").default_value("");
    // todo: when kv store is implemented, query the store instead
    parser.add_argument("--sim").help("Wait for simulated time message").default_value(false).implicit_value(true);
    parser.add_argument("--dry-run").help("Only parse the launch file, don't start").flag();
    parser.add_argument("launch_yaml").help("The launch file to launch.");
    // TODO: hack on argparse to allow trailing args to work nicely
    // parser.add_argument("launch_args").nargs(argparse::nargs_pattern::any);
    Commit();
  }

  bool HandleArgs(int argc, char *argv[], const std::vector<std::string> &leftover_args) {
    auto launch_yaml = parser.get("launch_yaml");
    std::vector<std::string> args(argv, argv + argc);
    basis::launch::LaunchContext context;
    context.process_filter = parser.get("--process");
    context.sim = parser.get<bool>("--sim");
    context.all_args = {argv, argv + argc};
    context.launch_args = leftover_args;

    auto launch = basis::launch::ParseTemplatedLaunchDefinitionYAMLPath(launch_yaml, leftover_args);
    if (!launch) {
      return false;
    }
    if (parser.get<bool>("--dry-run")) {
      std::cout << basis::launch::LaunchDefinitionToDebugString(*launch) << std::endl;
    } else {
      bool has_units = false;
      for (auto &[_, process] : launch->processes) {
        if (process.units.size()) {
          has_units = true;
          break;
        }
      }
      if (!has_units) {
        BASIS_LOG_FATAL("No units found in launch file");
        return false;
      }

      // TODO: check for empty definition (no units!)
      basis::launch::LaunchYamlDefinition(*launch, context);
    }
    return true;
  }
};
} // namespace basis::cli