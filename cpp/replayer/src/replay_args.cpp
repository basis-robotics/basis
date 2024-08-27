#include <basis/replayer/replay_args.h>

namespace basis::replayer {

std::unique_ptr<argparse::ArgumentParser> CreateArgumentParser() {
  auto parser = std::make_unique<argparse::ArgumentParser>("replay");

  
  parser->add_argument(LOOP_ARG).help("Whether or not to loop the playback.").default_value(false).implicit_value(true);
  // TODO: allow multiple mcap files, directory support
  parser->add_argument(RECORDING_ARG).help("The MCAP file to replay.");

  return parser;
}

} // namespace basis::replayer