
#include <basis/launch.h>
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
using namespace basis::launch;

const std::string test_file_name = "/test.launch.yaml";
CurrentLaunchParseState default_parse_state(test_file_name, 0);

RecordingSettings recording_settings = {
    .name = "camera_pipeline",
    .patterns = {glob::GlobToRegex("/log")},
    .directory = "/tmp/",
};
std::pair<std::string, basis::launch::UnitDefinition> foxglove_definition = {
    "/foxglove/foxglove", {.unit_type = "foxglove", .args = {}, .source_file = test_file_name}};
std::pair<std::string, basis::launch::UnitDefinition> v4l2_definition = {"/webcam/v4l2_camera_driver",
                                                                         {.unit_type = "v4l2_camera_driver",
                                                                          .args =
                                                                              {
                                                                                  {"device", "/dev/video0"},
                                                                                  {"topic_namespace", "/camera"},
                                                                              },
                                                                          .source_file = test_file_name}};
LaunchDefinition basic_definition = {.recording_settings = recording_settings,
                                     .processes = {{"/",
                                                    {.units =
                                                         {
                                                             foxglove_definition,
                                                             v4l2_definition,
                                                         },
                                                     .source_file = test_file_name}}}};

LaunchDefinition only_foxglove_definition = {.recording_settings = recording_settings,
                                             .processes = {{"/",
                                                            {.units =
                                                                 {
                                                                     foxglove_definition,
                                                                 },
                                                             .source_file = test_file_name}}}};

TEST(TestLaunchDefinition, Basic) {
  const std::string launch_file = R"(
recording:
  directory: /tmp/
  name: camera_pipeline
  topics:
    - /log
groups:
  foxglove:
    units:
      foxglove: {}
  webcam:
    units:
      v4l2_camera_driver:
        args:
          device: /dev/video0
          topic_namespace: /camera
)";

  auto parsed = ParseTemplatedLaunchDefinitionYAMLContents(launch_file, {}, default_parse_state);
  ASSERT_TRUE(parsed);
  basis::launch::LaunchDefinitionDebugFormatter outputter;
  ASSERT_EQ(LaunchDefinitionToDebugString(*parsed, outputter), LaunchDefinitionToDebugString(basic_definition, outputter));
  ASSERT_EQ(parsed->processes.at("/").source_file, basic_definition.processes.at("/").source_file);
  ASSERT_EQ(*parsed, basic_definition);
}

TEST(TestLaunchDefinition, TemplatedArgs) {
  const std::string basic_launch = R"(
recording:
  directory: /tmp/
  name: camera_pipeline
  topics:
    - /log
groups:
  foxglove:
    units:
      foxglove: {}
  webcam:
    units:
{% if args.enable_camera %}
      v4l2_camera_driver:
        args:
          device: /dev/video0
          topic_namespace: /camera
{% endif %}
)";
  {
    const std::string with_arg_no_default = R"(
args:
  enable_camera:
    type: bool
---
)" + basic_launch;

    auto fail_no_args = ParseTemplatedLaunchDefinitionYAMLContents(with_arg_no_default, {}, default_parse_state);
    ASSERT_FALSE(fail_no_args);
    auto fail_bad_type = ParseTemplatedLaunchDefinitionYAMLContents(
        with_arg_no_default, std::vector<std::pair<std::string, std::string>>{{"enable_camera", "this_is_not_a_bool"}},
        default_parse_state);
    ASSERT_FALSE(fail_bad_type);

    auto succeed_true = ParseTemplatedLaunchDefinitionYAMLContents(
        with_arg_no_default, std::vector<std::pair<std::string, std::string>>{{"enable_camera", "TrUe"}},
        default_parse_state);
    ASSERT_TRUE(succeed_true);
    ASSERT_EQ(*succeed_true, basic_definition);

    auto succeed_false = ParseTemplatedLaunchDefinitionYAMLContents(
        with_arg_no_default, std::vector<std::pair<std::string, std::string>>{{"enable_camera", "FALSE"}},
        default_parse_state);
    ASSERT_TRUE(succeed_false);
    ASSERT_EQ(*succeed_false, only_foxglove_definition);
  }
  {
    const std::string with_arg_default = R"(
args:
  enable_camera:
    type: bool
    default: True
---
)" + basic_launch;
    auto succeed_default = ParseTemplatedLaunchDefinitionYAMLContents(with_arg_default, {}, default_parse_state);
    ASSERT_TRUE(succeed_default);
    ASSERT_EQ(*succeed_default, basic_definition);
    auto succeed_true = ParseTemplatedLaunchDefinitionYAMLContents(
        with_arg_default, std::vector<std::pair<std::string, std::string>>{{"enable_camera", "TrUe"}},
        default_parse_state);
    ASSERT_TRUE(succeed_true);
    ASSERT_EQ(*succeed_true, basic_definition);

    auto succeed_false = ParseTemplatedLaunchDefinitionYAMLContents(
        with_arg_default, std::vector<std::pair<std::string, std::string>>{{"enable_camera", "FALSE"}},
        default_parse_state);
    ASSERT_TRUE(succeed_false);
    ASSERT_EQ(*succeed_false, only_foxglove_definition);
  }
}

TEST(TestLaunchDefinition, TestIncludes) {
  std::cout << std::filesystem::current_path() << std::endl;
  constexpr int NUM_INCLUDE_TEST_TYPES = 5;
  for (int i = 0; i < NUM_INCLUDE_TEST_TYPES; i++) {
    auto parsed = ParseTemplatedLaunchDefinitionYAMLPath(
        "test_include.launch.yaml",
        std::vector<std::pair<std::string, std::string>>{{"include_foxglove_type", std::to_string(i)}});
    ASSERT_TRUE(parsed);
    if (i != 0) {
      switch (i) {
      case 0:
        // No foxglove
        break;
      case 1:
      case 2:
        ASSERT_TRUE(parsed->processes.at("/").units.contains("/foxglove_inner/foxglove")) << i;
        break;
      case 3:
        ASSERT_TRUE(parsed->processes.at("/").units.contains("/foxglove/foxglove_inner/foxglove")) << i;
        break;
      case 4:
        for (auto &[k, v] : parsed->processes) {
          std::cout << k << std::endl;
        }
        ASSERT_TRUE(parsed->processes.contains("/foxglove/foxglove_inner")) << i;
        // ASSERT_TRUE(parsed->processes.at("/").units.contains("/foxglove/foxglove_inner/foxglove")) << i;
        break;
      }
    }
  }
}

// TODO: there are a number of edge cases we should test here (that were hand tested instead), work more at filling them
// out