#include <simple_sub.h>

using namespace unit::simple_sub;

OnChatter::Output simple_sub::OnChatter(const OnChatter::Input &input) {
  BASIS_LOG_INFO("OnChatter: {}", input.chatter->message());
  return OnChatter::Output();
}
