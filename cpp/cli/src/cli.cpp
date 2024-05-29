#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include <basis/core/coordinator_connector.h>

int main(int argc, char *argv[]) {
  argparse::ArgumentParser parser("basis");

  parser.add_argument("--port")
  .help("The port that the basis coordinator is listening at.")
    .scan<'i', uint16_t>()
    .default_value(BASIS_PUBLISH_INFO_PORT);

  // basis topic
  argparse::ArgumentParser topic_command("topic");
  topic_command.add_description("Topic information");
  // basis topic ls
  argparse::ArgumentParser topic_ls_command("ls");
  topic_ls_command.add_description("list the available topics");
  topic_command.add_subparser(topic_ls_command);
  // basis topic ls
  argparse::ArgumentParser topic_info_command("info");
  topic_info_command.add_description("get a topic's information");
  topic_info_command.add_argument("topic");
  topic_command.add_subparser(topic_info_command);
  // basis topic print
  argparse::ArgumentParser topic_print_command("print");
  topic_print_command.add_description("print a message on the topic");
  topic_print_command.add_argument("topic");
  topic_command.add_subparser(topic_print_command);

  parser.add_subparser(topic_command);

  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

auto port = parser.get<uint16_t>("--port");
  auto connection = basis::core::transport::CoordinatorConnector::Create(port);
  if(!connection) {
    spdlog::error("Unable to connect to the basis coordinator at port {}", port);
    return 1;
  }


  if(parser.is_subcommand_used("topic")) {
    if(topic_command.is_subcommand_used("ls")) {
    //    auto input = program.get<int>("square");
    }
  }

  return 0;
}
