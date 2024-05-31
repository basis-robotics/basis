#include <basis/plugins/serialization/rosmsg.h>


namespace basis::plugins::serialization {
    RosMsgParser::ParsersCollection<RosMsgParser::ROS_Deserializer> RosmsgSerializer::parser_collection;
}