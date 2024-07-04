#include <simple_sub.h>

using namespace unit::simple_sub;


SimpleSub::Output simple_sub::SimpleSub(const SimpleSub::Input& input) {
    spdlog::info("SimpleSub::SimpleSub: {}", input.chatter->message());
    return SimpleSub::Output();
}