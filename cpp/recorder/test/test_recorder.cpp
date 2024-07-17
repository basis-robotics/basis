#include <test.pb.h>

#include <gtest/gtest.h>

#include <basis/recorder.h>

#include <basis/plugins/serialization/protobuf.h>
#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#include <std_msgs/String.h>
#endif

#include "mcap/reader.hpp"

template <typename RecorderClass> class TestRecorderT : public testing::Test {
public:
  TestRecorderT() { InitializeMCAP(); }

  void InitializeMCAP() {
    char temp_template[] = "/tmp/tmpdir.XXXXXX";
    char *dir_name = mkdtemp(temp_template);

    spdlog::info("{} {}", ::testing::UnitTest::GetInstance()->current_test_info()->name(), dir_name);

    recorder = std::make_unique<RecorderClass>(dir_name);

    record_dir = dir_name;
  }

  bool Start(std::string_view output_name = "test") {
    current_mcap_name = std::string(output_name);
    return recorder->Start(output_name);
  }

  bool Split(std::string_view output_name) {
    current_mcap_name = std::string(output_name);
    return recorder->Split(output_name);
  }

  ~TestRecorderT() { CheckWrittenMCAP(); }

  void WriteMessage(const std::string &topic, const basis::OwningSpan &span) {
    ASSERT_TRUE(recorder->WriteMessage(topic, span, basis::core::MonotonicTime::Now()));
    message_counts_by_file_by_topic[current_mcap_name][topic]++;
  }

  void RegisterProtobuf(std::string topic_name = "/proto_topic") {
    auto basis_schema = basis::plugins::serialization::protobuf::ProtobufSerializer::DumpSchema<TestProtoStruct>();
    auto mti = basis::plugins::serialization::protobuf::ProtobufSerializer::DeduceMessageTypeInfo<TestProtoStruct>();
    recorder->RegisterTopic(topic_name, mti, basis_schema.schema_efficient);
  }

  void WriteProtobuf(std::string topic_name = "/proto_topic") {
    TestProtoStruct msg;

    msg.set_foo(3);
    msg.set_bar(8.5);
    msg.set_baz("baz");

    auto [bytes, size] = basis::SerializeToBytes(msg);
    std::shared_ptr<const std::byte[]> owning_bytes = std::move(bytes);
    std::span<const std::byte> view(owning_bytes.get(), size);
    WriteMessage(topic_name, {owning_bytes, view});
  }

  void RegisterAndWriteProtobuf() {
    RegisterProtobuf();
    WriteProtobuf();
  }

#ifdef BASIS_ENABLE_ROS
  void RegisterAndWriteRos() {
    spdlog::info("RegisterAndWriteRos");

    std_msgs::String msg;
    msg.data = "foobar";

    auto basis_schema = basis::plugins::serialization::RosmsgSerializer::DumpSchema<std_msgs::String>();
    auto mti = basis::plugins::serialization::RosmsgSerializer::DeduceMessageTypeInfo<std_msgs::String>();

    recorder->RegisterTopic("/ros_topic", mti, basis_schema.schema);

    auto [bytes, size] = basis::SerializeToBytes(msg);
    std::shared_ptr<const std::byte[]> owning_bytes = std::move(bytes);
    std::span<const std::byte> view(owning_bytes.get(), size);

    WriteMessage("/ros_topic", {owning_bytes, view});
  }
#endif

  void CheckWrittenMCAP() {
    recorder->Stop();

    for (auto &[name, counts] : message_counts_by_file_by_topic) {
      std::string filename = (record_dir / (name + ".mcap")).string();
      mcap::McapReader reader;
      ASSERT_TRUE(reader.open(filename).ok());
      ASSERT_TRUE(reader.readSummary(mcap::ReadSummaryMethod::NoFallbackScan).ok());
      auto stats = reader.statistics();
      ASSERT_NE(stats, std::nullopt);

      ASSERT_EQ(stats->channelMessageCounts.size(), counts.size());

      const auto channels = reader.channels();

      for (const auto &[channel_id, count] : stats->channelMessageCounts) {
        const std::string &topic = channels.at(channel_id)->topic;
        ASSERT_EQ(counts[topic], count);
      }
      // TODO: it would be even better to compare the message contents, but want to have a proper reader class first
    }
  }

  std::filesystem::path record_dir;

  std::unique_ptr<RecorderClass> recorder;

  std::string current_mcap_name;

  std::unordered_map<std::string, std::unordered_map<std::string, size_t>> message_counts_by_file_by_topic;
};

using TestRecorder = TestRecorderT<basis::Recorder>;

TEST_F(TestRecorder, BasicTest) {
  ASSERT_TRUE(Start());

  RegisterAndWriteProtobuf();
}

TEST_F(TestRecorder, OutOfOrder) {
  RegisterProtobuf();

  ASSERT_TRUE(Start());

  WriteProtobuf();
}

TEST_F(TestRecorder, Split) {
  for (char c = 'a'; c <= 'z'; c++) {
    RegisterProtobuf(fmt::format("/{}", c));
  }

  ASSERT_TRUE(Start());

  for (char c = 'a'; c <= 'z'; c++) {
    WriteProtobuf(fmt::format("/{}", c));
  }

  Split("test2");

  for (char c = 'z'; c >= 'a'; c--) {
    WriteProtobuf(fmt::format("/{}", c));
    WriteProtobuf(fmt::format("/{}", c));
  }
}

#ifdef BASIS_ENABLE_ROS

TEST_F(TestRecorder, Ros) {
  ASSERT_TRUE(Start());

  RegisterAndWriteRos();
}

TEST_F(TestRecorder, Mixed) {
  ASSERT_TRUE(Start());

  RegisterAndWriteProtobuf();
  RegisterAndWriteRos();
}

#endif

using TestAsyncRecorder = TestRecorderT<basis::AsyncRecorder>;

TEST_F(TestAsyncRecorder, BasicTest) {
  ASSERT_TRUE(Start());

  RegisterAndWriteProtobuf();
}

// TestOutOfOrder

// TestMixed