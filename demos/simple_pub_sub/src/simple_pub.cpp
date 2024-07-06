#include <simple_pub.h>

using namespace unit::simple_pub;

PublishAt1Hz::Output
simple_pub::PublishAt1Hz(const PublishAt1Hz::Input &input) {
  Logger().info("SimplePub::SimplePub");
  PublishAt1Hz::Output output;
  std::shared_ptr<StringMessage> msg{std::make_shared<StringMessage>()};
  msg->set_message(std::string("Hello, world!"));
  output.chatter = msg;
  return output;
}
