#include <test.pb.h>

#include <gtest/gtest.h>

#include <basis/recorder.h>

#include <basis/plugins/serialization/protobuf.h>
#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#include <std_msgs/String.h>
#endif

class TestRecorder : public testing::Test {
public:
  TestRecorder() {
    char temp_template[] = "/tmp/tmpdir.XXXXXX";
    char *dir_name = mkdtemp(temp_template);

    spdlog::info("{} {}", ::testing::UnitTest::GetInstance()->current_test_info()->name(), dir_name);

    recorder = std::make_unique<basis::Recorder>(dir_name);
  }

  void RegisterAndWriteProtobuf() {
    TestProtoStruct msg;

    msg.set_foo(3);
    msg.set_bar(8.5);
    msg.set_baz("baz");

    auto basis_schema = basis::plugins::serialization::ProtobufSerializer::DumpSchema<TestProtoStruct>();

    recorder->RegisterTopic(
        "/proto_topic", basis::plugins::serialization::ProtobufSerializer::GetMCAPMessageEncoding(), basis_schema.name,
        basis::plugins::serialization::ProtobufSerializer::GetMCAPSchemaEncoding(), basis_schema.schema_efficient);

    auto [bytes, size] = basis::SerializeToBytes(msg);

    recorder->WriteMessage("/proto_topic", {bytes.get(), size}, basis::core::MonotonicTime::Now());
  }

  void RegisterAndWriteRos() {
    std_msgs::String msg;
    msg.data = "foobar";

    auto basis_schema = basis::plugins::serialization::RosmsgSerializer::DumpSchema<std_msgs::String>();

    recorder->RegisterTopic("/ros_topic", basis::plugins::serialization::RosmsgSerializer::GetMCAPMessageEncoding(),
                            basis_schema.name, basis::plugins::serialization::RosmsgSerializer::GetMCAPSchemaEncoding(),
                            basis_schema.schema);

    auto [bytes, size] = basis::SerializeToBytes(msg);

    recorder->WriteMessage("/ros_topic", {bytes.get(), size}, basis::core::MonotonicTime::Now());
  }

  std::unique_ptr<basis::Recorder> recorder;
};

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

// TestOutOfOrder

// TestMixed