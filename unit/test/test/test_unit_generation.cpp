#include <gtest/gtest.h>

#include <test_unit.h>

#include <vector>

TEST(TestUnitGeneration, TestInitialization) { test_unit unit; }

template <class T, template <class...> class U>
inline constexpr bool is_instance_of_v = std::false_type{};

template <template <class...> class U, class... Vs>
inline constexpr bool is_instance_of_v<U<Vs...>, U> = std::true_type{};

// Check that we are single threaded
static_assert(std::is_base_of_v<basis::SingleThreadedUnit, test_unit>);

// Check that we set up all sync correctly
static_assert(is_instance_of_v<unit::test_unit::AllTest::Synchronizer,
                               basis::synchronizers::All>);
static_assert(
    std::is_same_v<
        std::tuple_element<
            0, unit::test_unit::AllTest::Synchronizer::MessageSumType>::type,
        std::shared_ptr<const ::TimeTest>>);
static_assert(
    std::is_same_v<
        std::tuple_element<
            1, unit::test_unit::AllTest::Synchronizer::MessageSumType>::type,
        std::shared_ptr<const sensor_msgs::Image>>);

TEST(TestUnitGeneration, TestAllHandler) {
  test_unit unit;

  bool callback_called = false;
  bool pub_callback_called = false;

  unit::test_unit::AllTest::PubSub pubsub(
      [&](auto input) {
        callback_called = true;
        return unit::test_unit::AllTest::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  pubsub.synchronizer->OnMessage<0>(std::make_shared<const ::TimeTest>());
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<1>(
      std::make_shared<const sensor_msgs::Image>());
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_TRUE(callback_called);
  ASSERT_TRUE(pub_callback_called);
}

// Check that we set up field sync correcty
static_assert(is_instance_of_v<unit::test_unit::StereoMatch::Synchronizer,
                               basis::synchronizers::FieldSync>);
static_assert(std::is_same_v<
              std::tuple_element<0, unit::test_unit::StereoMatch::Synchronizer::
                                        MessageSumType>::type,
              std::shared_ptr<const sensor_msgs::Image>>);
static_assert(std::is_same_v<
              std::tuple_element<1, unit::test_unit::StereoMatch::Synchronizer::
                                        MessageSumType>::type,
              std::shared_ptr<const sensor_msgs::Image>>);

TEST(TestUnitGeneration, TestFieldHandler) {
  test_unit unit;

  bool callback_called = false;
  bool pub_callback_called = false;

  unit::test_unit::StereoMatch::PubSub pubsub(
      [&](auto input) {
        callback_called = true;
        return unit::test_unit::StereoMatch::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  auto a = std::make_shared<sensor_msgs::Image>();
  a->header.stamp.sec = 1;
  auto b = std::make_shared<sensor_msgs::Image>();
  b->header.stamp.sec = 2;

  pubsub.synchronizer->OnMessage<0>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<1>(a);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<1>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_TRUE(callback_called);
  ASSERT_TRUE(pub_callback_called);
}

// TODO: check specifically against approximate field sync (template magic is
// hard)
static_assert(is_instance_of_v<unit::test_unit::ApproxTest::Synchronizer,
                               basis::synchronizers::FieldSync>);
static_assert(
    std::is_same_v<
        std::tuple_element<
            0, unit::test_unit::ApproxTest::Synchronizer::MessageSumType>::type,
        std::shared_ptr<const ::ExampleStampedVector>>);
static_assert(
    std::is_same_v<
        std::tuple_element<
            1, unit::test_unit::ApproxTest::Synchronizer::MessageSumType>::type,
        std::shared_ptr<const sensor_msgs::PointCloud2>>);

TEST(TestUnitGeneration, TestApproxHandler) {
  test_unit unit;

  bool callback_called = false;
  bool pub_callback_called = false;

  unit::test_unit::ApproxTest::PubSub pubsub(
      [&](auto input) {
        callback_called = true;
        return unit::test_unit::ApproxTest::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  auto v_a = std::make_shared<::ExampleStampedVector>();
  v_a->set_time(0.95);
  auto v_b = std::make_shared<::ExampleStampedVector>();
  v_b->set_time(0.991);
  auto p_a = std::make_shared<sensor_msgs::PointCloud2>();
  p_a->header.stamp.sec = 1;

  pubsub.synchronizer->OnMessage<0>(v_a);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<1>(p_a);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<0>(v_b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_TRUE(callback_called);
  ASSERT_TRUE(pub_callback_called);
}

static_assert(is_instance_of_v<unit::test_unit::TestEqualOptions::Synchronizer,
                               basis::synchronizers::FieldSync>);

//   /required_a:
//     # TODO: we may end up allowing the `rosmsg:` field to be dropped, either
//     by doing a search in the schemas # or by allowing a default serializer to
//     be specified type: rosmsg:sensor_msgs::Image # TODO: We may end up
//     providing default sync_fields for some serializer or message types to
//     reduce boilerplate sync_field: header

static_assert(std::is_same_v<
              std::tuple_element<0, unit::test_unit::TestEqualOptions::
                                        Synchronizer::MessageSumType>::type,
              std::shared_ptr<const sensor_msgs::Image>>);
//   /required_b:
//     type: rosmsg:sensor_msgs::Image
//     sync_field: header

static_assert(std::is_same_v<
              std::tuple_element<1, unit::test_unit::TestEqualOptions::
                                        Synchronizer::MessageSumType>::type,
              std::shared_ptr<const sensor_msgs::Image>>);

//   /buffered_optional:
//     type: protobuf:TestEmptyEvent
//     optional: True
//     accumulate: 10
static_assert(std::is_same_v<
              std::tuple_element<2, unit::test_unit::TestEqualOptions::
                                        Synchronizer::MessageSumType>::type,
              std::vector<std::shared_ptr<const TestEmptyEvent>>>);

//   /buffered_non_optional:
//     type: protobuf:TestEmptyEvent
//     accumulate: 10
static_assert(std::is_same_v<
              std::tuple_element<3, unit::test_unit::TestEqualOptions::
                                        Synchronizer::MessageSumType>::type,
              std::vector<std::shared_ptr<const TestEmptyEvent>>>);

//   /optional_but_sync:
//     type: rosmsg:sensor_msgs::Image
//     # TODO: We may end up providing default sync_fields for some serializer
//     or message types to reduce boilerplate sync_field: header
static_assert(std::is_same_v<
              std::tuple_element<4, unit::test_unit::TestEqualOptions::
                                        Synchronizer::MessageSumType>::type,
              std::shared_ptr<const sensor_msgs::Image>>);

//   /optional:
//     type: rosmsg:sensor_msgs::Image
//     # TODO: We may end up providing default sync_fields for some serializer
//     or message types to reduce boilerplate sync_field:
//     header
static_assert(std::is_same_v<
              std::tuple_element<5, unit::test_unit::TestEqualOptions::
                                        Synchronizer::MessageSumType>::type,
              std::shared_ptr<const sensor_msgs::Image>>);

TEST(TestUnitGeneration, TestEqualOptions) {
  test_unit unit;

  bool callback_called = false;
  bool pub_callback_called = false;

  unit::test_unit::TestEqualOptions::Input gotten_input;

  unit::test_unit::TestEqualOptions::PubSub pubsub(
      [&](auto input) {
        callback_called = true;
        gotten_input = input;
        return unit::test_unit::TestEqualOptions::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  auto a = std::make_shared<sensor_msgs::Image>();
  a->header.stamp.sec = 1;
  auto b = std::make_shared<sensor_msgs::Image>();
  b->header.stamp.sec = 2;

  pubsub.synchronizer->OnMessage<0>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<1>(a);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  pubsub.synchronizer->OnMessage<1>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);


  pubsub.synchronizer->OnMessage<3>(std::make_shared<TestEmptyEvent>());
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_TRUE(callback_called);
  ASSERT_TRUE(pub_callback_called);
  ASSERT_EQ(gotten_input.buffered_non_optional.size(), 1);
  ASSERT_EQ(gotten_input.optional_but_sync, nullptr);
  ASSERT_EQ(gotten_input.optional, nullptr);
  ASSERT_NE(gotten_input.required_a, nullptr);
  ASSERT_NE(gotten_input.required_b, nullptr);
    callback_called = false;
  pub_callback_called = false;
  gotten_input = {};

  pubsub.synchronizer->OnMessage<2>(std::make_shared<TestEmptyEvent>());
  pubsub.synchronizer->ConsumeIfReady();
  pubsub.synchronizer->OnMessage<2>(std::make_shared<TestEmptyEvent>());
  pubsub.synchronizer->ConsumeIfReady();

  pubsub.synchronizer->OnMessage<3>(std::make_shared<TestEmptyEvent>());
  pubsub.synchronizer->ConsumeIfReady();
  pubsub.synchronizer->OnMessage<3>(std::make_shared<TestEmptyEvent>());
  pubsub.synchronizer->ConsumeIfReady();
  pubsub.synchronizer->OnMessage<3>(std::make_shared<TestEmptyEvent>());
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  pubsub.synchronizer->OnMessage<4>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  pubsub.synchronizer->OnMessage<5>(a);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  pubsub.synchronizer->OnMessage<0>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  pubsub.synchronizer->OnMessage<1>(b);
  pubsub.synchronizer->ConsumeIfReady();
  ASSERT_TRUE(callback_called);
  ASSERT_TRUE(pub_callback_called);
  ASSERT_EQ(gotten_input.buffered_optional.size(), 2);
  ASSERT_EQ(gotten_input.buffered_non_optional.size(), 3);
  ASSERT_NE(gotten_input.optional_but_sync, nullptr);
  ASSERT_NE(gotten_input.optional, nullptr);
  ASSERT_NE(gotten_input.required_a, nullptr);
  ASSERT_NE(gotten_input.required_b, nullptr);

}