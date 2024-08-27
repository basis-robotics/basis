#include <argparse/argparse.hpp>
#include <memory>

namespace basis::replayer {

constexpr char LOOP_ARG[] = "--loop";
constexpr char RECORDING_ARG[] = "recording";

std::unique_ptr<argparse::ArgumentParser> CreateArgumentParser();

} // namespace basis::replayer