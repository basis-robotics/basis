#pragma once
#include "argparse/argparse.hpp"
#include "cli_subcommand.h"
#include <basis/cli_logger.h>
#include <basis/launch.h>
#include <basis/launch/launch_definition.h>
#include <basis/launch/mermaid_formatter.h>

namespace basis::cli {
class LaunchCommand : public CLISubcommand {
public:
  LaunchCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("launch", parent_parser) {
    parser.add_description("launch a yaml");
    parser.add_argument("--process").help("The process to launch inside the yaml").default_value("");
    // todo: when kv store is implemented, query the store instead
    parser.add_argument("--sim").help("Wait for simulated time message").default_value(false).implicit_value(true);
    parser.add_argument("--dry-run").help("Only parse the launch file, don't start").flag();
    parser.add_argument("--mermaid").help("Output a mermaid description of the graph").flag();
    parser.add_argument("launch_yaml_and_args")
        .help("The launch file to launch plus arguments.")
        .remaining()
        .nargs(argparse::nargs_pattern::at_least_one);

    Commit();
  }

  bool HandleArgs(int argc, char *argv[]) {
    const std::vector<std::string> launch_yaml_args = parser.get<std::vector<std::string>>("launch_yaml_and_args");

    const std::string &launch_yaml = launch_yaml_args[0];
    basis::launch::LaunchContext context;

    context.process_filter = parser.get("--process");
    context.sim = parser.get<bool>("--sim");
    context.all_args = {argv, argv + argc};
    context.launch_args = {++launch_yaml_args.begin(), launch_yaml_args.end()};

    auto launch = basis::launch::ParseTemplatedLaunchDefinitionYAMLPath(launch_yaml, context.launch_args);
    if (!launch) {
      return false;
    }

    const bool dry_run = parser.get<bool>("--dry-run") || parser.get<bool>("--mermaid");

    if (dry_run) {
      
      if(parser.get<bool>("--mermaid")) {
        launch::LaunchDefinitionMermaidFormatter outputter;
        std::cout << basis::launch::LaunchDefinitionToDebugString(*launch, outputter) << std::endl;
      }
      else {
        launch::LaunchDefinitionDebugFormatter outputter;
        std::cout << basis::launch::LaunchDefinitionToDebugString(*launch, outputter) << std::endl;
      }

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