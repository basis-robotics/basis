#pragma once
#include "cli_subcommand.h"
#include "fetch_schema.h"
#include <basis/core/coordinator_connector.h>

namespace basis::cli {

bool PrintSchema(const std::string &schema_name, basis::core::transport::CoordinatorConnector *connector) {
  std::optional<basis::core::transport::proto::MessageSchema> maybe_schema = FetchSchema(schema_name, connector, 5);
  if (maybe_schema) {
    std::cout << maybe_schema->serializer() << ":" << maybe_schema->name() << std::endl;
    std::cout << maybe_schema->schema() << std::endl;
    return true;
  }
  return false;
}

class SchemaPrintCommand : public CLISubcommand {
public:
  SchemaPrintCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("print", parent_parser) {
    parser.add_description("print a schema");
    parser.add_argument("schema");
    Commit();
  }
  bool HandleArgs(basis::core::transport::CoordinatorConnector *connector) {
    return PrintSchema(parser.get("schema"), connector);
  }
};
class SchemaCommand : public CLISubcommand {
public:
  SchemaCommand(argparse::ArgumentParser &parent_parser)
      : CLISubcommand("schema", parent_parser), schema_print_command(parser) {
    parser.add_description("Schema information");
    Commit();
  };

  bool HandleArgs(uint16_t port) {
    auto connector = CreateCoordinatorConnector(port);
    if(!connector) {
      return false;
    }
    if (schema_print_command.IsInUse()) {
      return schema_print_command.HandleArgs(connector.get());
    }
    return false;
  }

protected:
  // basis schema ls
  // todo
  // basis schema print
  SchemaPrintCommand schema_print_command;
};
} // namespace basis::cli