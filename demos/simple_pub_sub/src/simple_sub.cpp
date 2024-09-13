#include <simple_sub.h>

#include <thread>

using namespace unit::simple_sub;

OnChatter::Output simple_sub::OnChatter(const OnChatter::Input &input) {
  BASIS_LOG_INFO("OnChatter: {}", input.chatter->message());
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  return OnChatter::Output();
}
