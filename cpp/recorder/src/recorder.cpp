#define MCAP_IMPLEMENTATION

#include <basis/recorder.h>


namespace basis {
  const std::vector<std::regex> Recorder::RECORD_ALL_TOPICS = {
    std::regex(".*")
  };
 
}