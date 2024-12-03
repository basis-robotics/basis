#pragma once

#include "launch_definition.h"

namespace basis::launch {
struct LaunchDefinitionMermaidFormatter : public LaunchDefinitionDebugFormatter {
  virtual std::string HandleStart() override {
return R"""(---
config:
  flowchart:
    nodeSpacing: 5
    subGraphTitleMargin:
      bottom: 10
  defaultRenderer: elk
  elk:
      mergeEdges: True
---
graph LR
  %%{init: {"flowchart": {"defaultRenderer": "elk"}} }%%
)""";}

  virtual std::string HandleEnd() override;
  virtual std::string FormatUnit(std::string_view unit_name, const UnitDefinition& unit) override;
  virtual std::string FormatProcess(std::string_view process_name, std::vector<std::string> unit_cmds) override;


  std::unordered_map<std::string, std::vector<std::string>> handlers_with_input;
  std::unordered_map<std::string, std::vector<std::string>> handlers_with_output;

};
}