/*

  This is the starting point for your Unit. Edit this directly and implement the missing methods!

*/

#include <simple_pub.h>

using namespace unit::simple_pub;


SimplePub::Output simple_pub::SimplePub(const SimplePub::Input& input) {
    spdlog::info("SimplePub::SimplePub");
    SimplePub::Output output;
    std::shared_ptr<StringMessage> msg{std::make_shared<StringMessage>()};
    msg->set_message(std::string("Hello, world!"));
    output.chatter = msg;
    return output;
}
