#include <string_view>

#include <vector>

// todo: probably take a std::fs::path here
void LaunchYamlPath(std::string_view yaml_path, const std::vector<std::string>& args, std::string process_name_filter="");