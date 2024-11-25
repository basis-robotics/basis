#include <basis/launch.h>
#include <basis/launch/unit_loader.h>
#include <basis/unit.h>
#include <basis/launch/mermaid_formatter.h>

#include "spdlog/fmt/bundled/format.h"

namespace basis::launch {
std::string LaunchDefinitionMermaidFormatter::FormatUnit(std::string_view unit_name, const UnitDefinition& unit) {
  /**
TODO: the correct thing to do here is to find the associated .yaml file, parse it, and run inja to turn any templated topics into real topics
Doing it this way means we end up calling the constructor for the unit, which might not be safe (due to requirements for hardware, etc)
  */
  std::optional<std::filesystem::path> unit_so_path = FindUnit(unit.unit_type);
  if (!unit_so_path) {
    BASIS_LOG_FATAL("Failed to find unit type {}", unit.unit_type);
    // TODO: nicer error handling
    throw std::runtime_error("Failed to find unit type " + unit.unit_type);
  }

  std::unique_ptr<basis::Unit> runtime_unit(CreateUnitWithLoader(*unit_so_path, unit_name, unit.args));
          runtime_unit->CreateTransportManager(nullptr);

  runtime_unit->Initialize({.create_subscribers = false});

  std::vector<std::string> handlers;
  for(const auto& [handler_name, handler] : runtime_unit->GetHandlers()) {
    std::string handler_full_name = fmt::format("handler_{}::{}", unit_name, handler_name);
    handlers.emplace_back(fmt::format("    {}[[\"{}()\"]]", handler_full_name, handler_name));
    for (const auto &input_name : handler->type_erased_callbacks) {
      handlers_with_input[input_name.first].emplace_back(handler_full_name);
    }
    
    for (const auto &output_name : handler->outputs) {
      handlers_with_output[output_name].emplace_back(handler_full_name);
    }
  }
  if(handlers.size() == 0) {
    handlers.emplace_back(fmt::format("    handler_{}:::hidden", unit_name));
  }
  
  return fmt::format("  subgraph unit_{}[\"{}\"]\n{}\n  end\n", unit_name, unit_name, fmt::join(handlers, "\n"));
}

std::string LaunchDefinitionMermaidFormatter::FormatProcess(std::string_view, std::vector<std::string> unit_cmds) {
  return fmt::format("{}", fmt::join(unit_cmds, "\n"));
}

  std::string LaunchDefinitionMermaidFormatter::HandleEnd() {
    std::vector<std::string> connections;
    for (const auto &[topic, inputs] : handlers_with_input) {
      for (const auto& input : inputs) {
        const auto& output_it = handlers_with_output.find(topic);
        if(output_it == handlers_with_output.end()) {
          // Create broken connection
          // Note: mermaid doesn't support X on the start of the arrow...
          connections.emplace_back(fmt::format("{}::{}:::hidden x--{}--> {}", input, topic, topic, input));
        }
        else {
          for (const auto& output : output_it->second) {
            connections.emplace_back(fmt::format("{} --{}--> {}", output, topic, input));
          }
        }
      }
    }
    
    for (const auto &[topic, outputs] : handlers_with_output) {           
      for (const auto& output : outputs) {
        if(!handlers_with_input.contains(topic)) {
          // Create broken connection
          connections.emplace_back(fmt::format("{} --{}--x {}::{}:::hidden", output, topic, topic, output));
          }
      }
    }
    return fmt::format("{}", fmt::join(connections, "\n"));
    }
}