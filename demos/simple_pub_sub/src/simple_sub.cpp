#include <simple_sub.h>

using namespace unit::simple_sub;

OnChatter::Output simple_sub::OnChatter(const OnChatter::Input &input) {
  spdlog::info("simple_sub::OnChatter: {}", input.chatter->message());
  return OnChatter::Output();
}
