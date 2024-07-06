#include <chrono>
#include <signal.h>
#include <sys/wait.h>

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

  bool Wait(int timeout_s = -1) {
    // Don't wait if we're empty or already waited
    if (pid == -1) {
      return true;
    }

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);

    int wait_ret = waitpid(pid, nullptr, timeout_s >= 0 ? /* has timeout, just check */ WNOHANG : /* no timeout, wait forever*/ 0);
    
    // Loop until we have a successful wait or time out
    while(wait_ret <= 0 && end > std::chrono::steady_clock::now()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      wait_ret = waitpid(pid, nullptr, WNOHANG);
    }

    // We hit an error, bail    
    if (wait_ret < 0) {
      spdlog::error("waitpid({}): {}", pid, strerror(errno));
    }
    if (wait_ret > 0) {
      pid = -1;
    }

    return pid == -1;
  }

  void KillAndWait() {
    if (pid == -1) {
      return;
    }

    // todo BASIS-21: Need to work from SIGINT SIGKILL etc
    kill(pid, SIGINT);

    Wait(5);

  }

private:
  int pid = -1;
  ProcessConfig config;
};
