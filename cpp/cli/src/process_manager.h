#pragma once

#include <utility>

namespace basis::cli {

struct ProcessConfig {
  /**
   * If set, will kill and wait for process death on shutdown
   */
  bool kill_on_shutdown = true;
};

/**
 * Class to manage an external process, including killing on death.
 */
class Process {
public:
  Process(int pid, ProcessConfig config = {}) : pid(pid), config(config) {}
  // No copying
  Process(const Process &) = delete;
  Process &operator=(const Process &) = delete;
  // Moving is fine
  Process(Process &&other) {
    *this = std::move(other);
  }
  Process &operator=(Process &&other) {
    if (this != &other) {
      // When we do move, if we have a process, kill it.
      Shutdown();
      // Copy the other pid and config over
      pid = other.pid;
      config = other.config;
      // And clear from the donor
      other.pid = -1;
    }
    return *this;
  }

  ~Process() { Shutdown(); }

  int GetPid() const {
    return pid;
  }

  void Shutdown() {
    if (config.kill_on_shutdown) {
      KillAndWait();
    }
  }

  bool Wait(int timeout_s = -1);

  void Kill(int sig);

  void KillAndWait();

private:
  int pid = -1;
  ProcessConfig config;
};

}