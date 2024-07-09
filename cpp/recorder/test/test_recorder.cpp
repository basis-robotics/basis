#include <test.pb.h>

#include <gtest/gtest.h>

#include <basis/recorder.h>

#include <basis/plugins/serialization/protobuf.h>
#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#include <std_msgs/String.h>
#endif

template<typename RecorderClass>
class TestRecorderT : public testing::Test {
public:
  TestRecorderT() {
    char temp_template[] = "/tmp/tmpdir.XXXXXX";
    char *dir_name = mkdtemp(temp_template);

    spdlog::info("{} {}", ::testing::UnitTest::GetInstance()->current_test_info()->name(), dir_name);

    recorder = std::make_unique<RecorderClass>(dir_name);
  }

  void RegisterAndWriteProtobuf() {
    spdlog::info("RegisterAndWriteProtobuf");

    TestProtoStruct msg;


    msg.set_foo(3);
    msg.set_bar(8.5);
    msg.set_baz("baz");

    auto basis_schema = basis::plugins::serialization::ProtobufSerializer::DumpSchema<TestProtoStruct>();

    recorder->RegisterTopic(
        "/proto_topic", basis::plugins::serialization::ProtobufSerializer::GetMCAPMessageEncoding(), basis_schema.name,
        basis::plugins::serialization::ProtobufSerializer::GetMCAPSchemaEncoding(), basis_schema.schema_efficient);

    auto [bytes, size] = basis::SerializeToBytes(msg);
    std::shared_ptr<const std::byte[]> owning_bytes = std::move(bytes);
    std::span<const std::byte> view(owning_bytes.get(), size);
    ASSERT_TRUE(recorder->WriteMessage("/proto_topic", {owning_bytes, view}, basis::core::MonotonicTime::Now()));
  }

  void RegisterAndWriteRos() {
    spdlog::info("RegisterAndWriteRos");
    
    std_msgs::String msg;
    msg.data = "foobar";

    auto basis_schema = basis::plugins::serialization::RosmsgSerializer::DumpSchema<std_msgs::String>();

    recorder->RegisterTopic("/ros_topic", basis::plugins::serialization::RosmsgSerializer::GetMCAPMessageEncoding(),
                            basis_schema.name, basis::plugins::serialization::RosmsgSerializer::GetMCAPSchemaEncoding(),
                            basis_schema.schema);

    auto [bytes, size] = basis::SerializeToBytes(msg);

    ASSERT_TRUE(recorder->WriteMessage("/ros_topic", {bytes.get(), size}, basis::core::MonotonicTime::Now()));
  }

  std::unique_ptr<RecorderClass> recorder;
};

using TestRecorder = TestRecorderT<basis::Recorder>;

TEST_F(TestRecorder, BasicTest) {
  ASSERT_TRUE(recorder->Start("test"));

  RegisterAndWriteProtobuf();

  // TODO: validate output
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

  // TODO: validate output
}


// TestOutOfOrder

// TestMixed