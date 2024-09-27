#include <basis/unit/args_template.h>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>

namespace basis::unit {

nonstd::expected<std::unordered_map<std::string, std::string>, std::string>
ParseTemplatedTopics(const UnitArgumentsBase &args, const std::vector<std::string> &topics) {
  nlohmann::json template_data;
  template_data["args"] = args.GetArgumentMapping();
  std::unordered_map<std::string, std::string> out;
  for (const std::string &topic : topics) {
    if (!out.contains(topic)) {
      try {
        auto rendered = inja::render(topic, template_data);
        out.emplace(topic, rendered);

        // Do some basic validation to protect against common issues
        // TODO: hoist this out, use in transport manager
        if (rendered.empty()) {
          return nonstd::make_unexpected(fmt::format("{}: topic resolved to empty string", topic));
        }
        if (rendered[0] != '/') {
          return nonstd::make_unexpected(fmt::format("{} -> {}: topic must start with /", topic, rendered));
        }
        if (rendered[rendered.size() - 1] == '/') {
          return nonstd::make_unexpected(fmt::format("{} -> {}: topic must not end with /", topic, rendered));
        }
        for (char c : rendered) {
          if (!(isalpha(c) || isdigit(c) || c == '_' || '/')) {
            return nonstd::make_unexpected(fmt::format("{} -> {}: topic must not end with /", topic, rendered));
          }
        }
      } catch (const std::exception &err) {
        return nonstd::make_unexpected(fmt::format("{}: {}", topic, err.what()));
      }
    }
  }
  return out;
}
} // namespace basis::unit