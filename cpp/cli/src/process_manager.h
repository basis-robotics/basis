#include <chrono>
#include <signal.h>
#include <sys/wait.h>

struct ProcessConfig {
  bool kill_on_shutdown = true;
};

class Process {
public:
  Process(int pid, ProcessConfig config = {}) : pid(pid), config(config) {}
  Process(const Process &) = delete;
  Process &operator=(const Process &) = delete;
  Process(Process &&other) {
    *this = std::move(other);
  }
  Process &operator=(Process &&other) {
    if (this != &other) {
      Shutdown();
      pid = other.pid;
      config = other.config;
      other.pid = -1;
    }
    return *this;
  }

  ~Process() { Shutdown(); }

  void Shutdown() {
    if (config.kill_on_shutdown) {
      KillAndWait();
    }
  }

  void Wait(int timeout_s = 0) {
    int wait_ret = waitpid(pid, nullptr, timeout_s ? WNOHANG : 0);
    
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);

    while(wait_ret <= 0 && end > std::chrono::steady_clock::now()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      wait_ret = waitpid(pid, nullptr, WNOHANG);
    }

    if (wait_ret < 0) {
      spdlog::error("waitpid({}): {}", pid, strerror(errno));
    }
    if (wait_ret > 0) {
      pid = -1;
    }
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
