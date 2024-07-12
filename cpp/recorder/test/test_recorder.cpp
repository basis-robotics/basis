#include <test.pb.h>

#include <gtest/gtest.h>

#include <basis/recorder.h>

#include <basis/plugins/serialization/protobuf.h>
#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#include <std_msgs/String.h>
#endif

#include "mcap/reader.hpp"

template<typename RecorderClass>
class TestRecorderT : public testing::Test {
public:
  TestRecorderT() {
    char temp_template[] = "/tmp/tmpdir.XXXXXX";
    char *dir_name = mkdtemp(temp_template);

    spdlog::info("{} {}", ::testing::UnitTest::GetInstance()->current_test_info()->name(), dir_name);

    recorder = std::make_unique<RecorderClass>(dir_name);

    record_dir = dir_name;
  }
  
  ~TestRecorderT() {
    CheckWrittenMCAP();
  }

  void WriteMessage(const std::string& topic, const basis::OwningSpan& span) {
    ASSERT_TRUE(recorder->WriteMessage(topic, span, basis::core::MonotonicTime::Now()));
    message_counts[topic]++;
  }

  void RegisterAndWriteProtobuf() {
    TestProtoStruct msg;

    msg.set_foo(3);
    msg.set_bar(8.5);
    msg.set_baz("baz");

    auto basis_schema = basis::plugins::serialization::ProtobufSerializer::DumpSchema<TestProtoStruct>();
    auto mti = basis::plugins::serialization::ProtobufSerializer::DeduceMessageTypeInfo<TestProtoStruct>();
    recorder->RegisterTopic(
        "/proto_topic", mti, basis_schema.schema_efficient);

    auto [bytes, size] = basis::SerializeToBytes(msg);
    std::shared_ptr<const std::byte[]> owning_bytes = std::move(bytes);
    std::span<const std::byte> view(owning_bytes.get(), size);
    WriteMessage("/proto_topic", {owning_bytes, view});
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

    std::string filename = (record_dir / "test.mcap").string();
    mcap::McapReader reader;
    ASSERT_TRUE(reader.open(filename).ok());
    ASSERT_TRUE(reader.readSummary(mcap::ReadSummaryMethod::NoFallbackScan).ok());
    auto stats = reader.statistics();
    ASSERT_NE(stats, std::nullopt);

    ASSERT_EQ(stats->channelMessageCounts.size(), message_counts.size());

    const auto channels = reader.channels(); 

    for(const auto& [channel_id, count] : stats->channelMessageCounts) {
      const std::string& topic = channels.at(channel_id)->topic;
      ASSERT_EQ(message_counts[topic], count);
    }
    // TODO: it would be even better to compare the message contents, but want to have a proper reader class first
  }

  std::filesystem::path record_dir;

  std::unique_ptr<RecorderClass> recorder;

  std::unordered_map<std::string, size_t> message_counts;
};

using TestRecorder = TestRecorderT<basis::Recorder>;

TEST_F(TestRecorder, BasicTest) {
  ASSERT_TRUE(recorder->Start("test"));

  RegisterAndWriteProtobuf();
}

#ifdef BASIS_ENABLE_ROS

TEST_F(TestRecorder, Ros) {
  ASSERT_TRUE(recorder->Start("test"));

  RegisterAndWriteRos();
}

TEST_F(TestRecorder, Mixed) {
  ASSERT_TRUE(recorder->Start("test"));

  RegisterAndWriteProtobuf();
  RegisterAndWriteRos();
}


#endif

using TestAsyncRecorder = TestRecorderT<basis::AsyncRecorder>;

TEST_F(TestAsyncRecorder, BasicTest) {
  ASSERT_TRUE(recorder->Start("test"));

  RegisterAndWriteProtobuf();
}


// TestOutOfOrder

// TestMixed