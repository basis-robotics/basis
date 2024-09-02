#include <simple_sub.h>
       #include <unistd.h>


using namespace unit::simple_sub;

OnChatter::Output simple_sub::OnChatter(const OnChatter::Input &input) {
  BASIS_LOG_INFO("OnChatter: {}", input.chatter->message());
  BASIS_LOG_INFO("Sleeping for 1s");
  sleep(1);
  BASIS_LOG_INFO("Post sleep");
  return OnChatter::Output();
}
