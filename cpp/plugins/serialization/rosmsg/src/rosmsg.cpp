#include <basis/plugins/serialization/rosmsg.h>

namespace basis::plugins::serialization {
RosMsgParser::ParsersCollection<RosMsgParser::ROS_Deserializer> RosmsgSerializer::parser_collection;
}

extern "C" {

basis::core::serialization::SerializationPlugin *LoadPlugin() {
  return new basis::plugins::serialization::RosMsgPlugin();
}
}