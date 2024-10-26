#pragma once
#include <argparse/argparse.hpp>
#include <string_view>

namespace basis::cli {
class CLISubcommand {
protected:
  CLISubcommand(const std::string &name, argparse::ArgumentParser &parent_parser)
      : name(name), parent_parser(parent_parser), parser(name) {}

public:
  bool IsInUse() const { return parent_parser.is_subcommand_used(name); }

  void Commit() { parent_parser.add_subparser(parser); }

protected:
  const std::string name;

private:
  argparse::ArgumentParser &parent_parser;

protected:
  argparse::ArgumentParser parser;
};
} // namespace basis::cli