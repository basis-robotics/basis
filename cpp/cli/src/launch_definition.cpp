#include "argparse/argparse.hpp"
#include "basis/core/logging/macros.h"
#include "yaml-cpp/eventhandler.h"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/parser.h"
#include <basis/launch/launch_definition.h>

#include <basis/launch.h>

#include <fstream>

#include <inja/inja.hpp>

#include <basis/arguments.h>

namespace basis::launch {

RecordingSettings ParseRecordingSettingsYAML(const YAML::Node &yaml) {
  RecordingSettings settings;

  if (yaml["directory"]) {
    settings.directory = yaml["directory"].as<std::string>();
  }

  for (const auto &pattern_yaml : yaml["topics"]) {
    settings.patterns.emplace_back(glob::GlobToRegex(pattern_yaml.as<std::string>()));
  }

  if (yaml["async"]) {
    settings.async = yaml["async"].as<bool>();
  }

  return settings;
}

std::optional<LaunchDefinition> ParseTemplatedLaunchDefinitionYAMLPath(std::filesystem::path yaml_path,
                                                                       const LaunchContext &context) {
  // Thanks stack overflow

  std::ifstream file(yaml_path.string(), std::ios::binary | std::ios::ate);
  if (file.fail()) {
    // TODO: maybe use ifstream::exceptions
    BASIS_LOG_ERROR("Launch file {} isn't openable.", yaml_path.string());
    return {};
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (file.read(buffer.data(), size)) {
    return ParseTemplatedLaunchDefinitionYAMLContents({buffer.data(), (size_t)size}, context);
  }
  BASIS_LOG_ERROR("Failed to load file {}", yaml_path.string());
  return {};
}

namespace {
using namespace YAML;
class DocumentFinder : public YAML::EventHandler {
public:
  virtual ~DocumentFinder() = default;

  virtual void OnDocumentStart(const Mark &mark) { document_start = mark.pos; }
  virtual void OnDocumentEnd() {}

  virtual void OnNull([[maybe_unused]] const Mark &mark, [[maybe_unused]] anchor_t anchor) {};
  virtual void OnAlias([[maybe_unused]] const Mark &mark, [[maybe_unused]] anchor_t anchor) {};
  virtual void OnScalar([[maybe_unused]] const Mark &mark, [[maybe_unused]] const std::string &tag,
                        [[maybe_unused]] anchor_t anchor, [[maybe_unused]] const std::string &value) {};

  virtual void OnSequenceStart([[maybe_unused]] const Mark &mark, [[maybe_unused]] const std::string &tag,
                               [[maybe_unused]] anchor_t anchor, [[maybe_unused]] EmitterStyle::value style) {};
  virtual void OnSequenceEnd() {};

  virtual void OnMapStart([[maybe_unused]] const Mark &mark, [[maybe_unused]] const std::string &tag,
                          [[maybe_unused]] anchor_t anchor, [[maybe_unused]] EmitterStyle::value style) {};
  virtual void OnMapEnd() {};

  virtual void OnAnchor([[maybe_unused]] const Mark & /*mark*/, [[maybe_unused]] const std::string & /*anchor_name*/) {}
  size_t document_start = 0;
};
} // namespace
std::optional<LaunchDefinition> ParseTemplatedLaunchDefinitionYAMLContents(std::string_view yaml_contents,
                                                                           const nlohmann::json &template_data,
                                                                           const LaunchContext &context);
std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLContents([[maybe_unused]] std::string_view yaml_contents,
                                           const LaunchContext &context) {
  nlohmann::json template_data;

  // Now separate out arguments from templated content
  // TODO cpp 23 https://en.cppreference.com/w/cpp/io/basic_ispanstream to avoid copy
  std::string yaml_contents_copy = std::string(yaml_contents);
  std::stringstream stream(yaml_contents_copy);
  auto yaml_parser = YAML::Parser(stream);

  DocumentFinder document_finder;
  // Skip the first document, which must be args, if multiple exist
  yaml_parser.HandleNextDocument(document_finder);

  std::unique_ptr<argparse::ArgumentParser> argparser;
  // Parse args, convert
  std::vector<std::string> launch_args;
  launch_args.reserve(context.launch_args.size() + 1);
  launch_args.push_back("");
  launch_args.insert(launch_args.end(), context.launch_args.begin(), context.launch_args.end());

  std::vector<std::unique_ptr<arguments::ArgumentMetadataBase>> argument_metadata;

  const size_t first_doc_marker = document_finder.document_start;
  try {
    yaml_parser.HandleNextDocument(document_finder);
  } catch (std::exception &) {
    // Due to templating causing invalid yaml, this might not succeed - but we will still get a document start marker
  }

  // Look for the content document
  if (first_doc_marker != document_finder.document_start) {
    // It exists
    const size_t template_content_start = document_finder.document_start;

    // The first document in the launch file with two documents cannot contain inja declarations, load it
    YAML::Node first_document = YAML::Load(yaml_contents_copy);

    if (YAML::Node args_yaml = first_document["args"]) {
      // Now, parse the args
      for (const auto &arg_name_yaml : args_yaml) {
        const auto &arg_name = arg_name_yaml.first.as<std::string>();
        const auto &arg_yaml = arg_name_yaml.second;
        // Help is optional
        std::string help;
        if (arg_yaml["help"]) {
          help = arg_yaml["help"].as<std::string>();
        }
        // default value is optional
        std::optional<std::string> default_value;
        if (arg_yaml["default"]) {
          default_value = arg_yaml["default"].as<std::string>();
        }

        bool optional = arg_yaml["optional"] && arg_yaml["optional"].as<bool>();
        if (optional && default_value) {
          BASIS_LOG_FATAL("argument {} cannot have both optional and a default value", arg_name);
        }
        auto argument = arguments::StringArgumentMetadata::Create(arg_name, help, arg_yaml["type"].as<std::string>(),
                                                                  optional, default_value);
        if (!argument) {
          BASIS_LOG_FATAL("Failed to parse args[{}]", arg_name);
          return {};
        }
        argument_metadata.push_back(std::move(argument));
      }
      argparser = arguments::ArgumentsBase::CreateArgumentParser(argument_metadata);
    }
    // Adjust the pointer to the start of templated content
    yaml_contents = yaml_contents.substr(template_content_start);
  }

  // If we specified no arguments, create an empty argparser so that we get consistent error messages
  if (!argparser) {
    argparser = std::make_unique<argparse::ArgumentParser>();
  }

  try {
    argparser->parse_args(launch_args);
  } catch (const std::exception &err) {
    // argparse uses exceptions for error handling - handle it and convert to an error
    BASIS_LOG_FATAL("{}", err.what());

    BASIS_LOG_FATAL("{}", argparser->help().str());
    return {};
  }

  for (const auto &metadata : argument_metadata) {
    metadata->type_metadata.to_json(*argparser, metadata->name, template_data[metadata->name]);
  }
  return ParseTemplatedLaunchDefinitionYAMLContents(yaml_contents, template_data, context);
}

std::optional<LaunchDefinition>
ParseTemplatedLaunchDefinitionYAMLContents(std::string_view yaml_contents, const nlohmann::json &template_data,
                                           [[maybe_unused]] const LaunchContext &command_line) {
  inja::Environment inja_env;
  inja_env.set_throw_at_missing_includes(true);

  std::string rendered_contents;
  try {
    rendered_contents = inja_env.render(yaml_contents, template_data);
  } catch (const std::exception &e) {
    BASIS_LOG_FATAL("Failed to render templated launch file: {}", e.what());
    return {};
  }
  try {
    // TODO: will need to show the parsed jinja on error
    auto node = YAML::Load(rendered_contents);
    if (node["args"]) {
      // This is always an error - we want clear separation between templated and non-templated data
      BASIS_LOG_FATAL("args detected inline with launch content, required to be separated by document marker '---'");
      return {};
    }
    return ParseLaunchDefinitionYAML(node);

  } catch (const std::exception &e) {
    BASIS_LOG_FATAL("Failed to parse launch file: {}", e.what());
    return {};
  }
}

LaunchDefinition ParseLaunchDefinitionYAML(const YAML::Node &yaml) {
  LaunchDefinition launch;

  for (const auto &process_yaml : yaml["processes"]) {
    ProcessDefinition process;
    for (const auto &unit_name_yaml : process_yaml.second["units"]) {
      const auto &unit_name = unit_name_yaml.first.as<std::string>();
      const auto &unit_yaml = unit_name_yaml.second;
      UnitDefinition unit;

      if (unit_yaml["unit"]) {
        // If we specify a unit type, use that
        unit.unit_type = unit_yaml["unit"].as<std::string>();
      } else {
        // By default, the unit name is the unit type
        unit.unit_type = unit_name;
      }

      if (unit_yaml["args"]) {
        for (const auto &kv : unit_yaml["args"]) {
          std::cout << kv.first.as<std::string>() << std::endl;
          unit.args.emplace_back(std::pair{kv.first.as<std::string>(), kv.second.as<std::string>()});
        }
      }

      process.units.emplace("/" + unit_name, std::move(unit));
    }
    launch.processes.emplace(process_yaml.first.as<std::string>(), std::move(process));
  }
  if (yaml["recording"]) {
    launch.recording_settings = ParseRecordingSettingsYAML(yaml["recording"]);
  }
  return launch;
}

} // namespace basis::launch