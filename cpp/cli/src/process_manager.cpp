#include "process_manager.h"

#include <chrono>
#include <signal.h>
#include <sys/wait.h>
#include <thread>

#include <basis/cli_logger.h>

namespace basis::cli {

bool Process::Wait(int timeout_s) {
  // Don't wait if we're empty or already waited
  if (pid == -1) {
    return true;
  }

  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);

  int wait_ret =
      waitpid(pid, nullptr, timeout_s >= 0 ? /* has timeout, just check */ WNOHANG : /* no timeout, wait forever*/ 0);

  // Loop until we have a successful wait or time out
  while (wait_ret <= 0 && end > std::chrono::steady_clock::now()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    wait_ret = waitpid(pid, nullptr, WNOHANG);
  }

  // We hit an error, bail
  if (wait_ret < 0) {
    BASIS_LOG_ERROR("waitpid({}): {}", pid, strerror(errno));
  }
  if (wait_ret > 0) {
    pid = -1;
  }

  return pid == -1;
}

void Process::Kill(int sig) {
  if (pid == -1) {
    return;
  }

  kill(pid, sig);
}

void Process::KillAndWait() {
  if (pid == -1) {
    return;
  }
  // todo BASIS-21: Need to work from SIGINT SIGKILL etc
  Kill(SIGHUP);

  Wait(5);
}

} // namespace basis::cli