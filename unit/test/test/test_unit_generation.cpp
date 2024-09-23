#include <basis/core/coordinator.h>
#include <basis/core/time.h>
#include <gtest/gtest.h>

#include <test_unit.h>

#include <vector>

#include <spdlog/spdlog.h>

spdlog::logger logger("test_unit_generation");

template <typename T_PUBSUB> auto CreateCallbacks(T_PUBSUB &pubsub) {
  return [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::make_tuple(pubsub.template CreateOnMessageCallback<I, false>()...);
  }(std::make_index_sequence<T_PUBSUB::input_count>());
}

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

  unit::test_unit::AllTest::PubSub pubsub(&logger,
      [&](auto input) {
        callback_called = true;
        return unit::test_unit::AllTest::Output();
      },
      [&](auto output) { pub_callback_called = true; });
  auto cbs = CreateCallbacks(pubsub);
  std::get<0>(cbs)(std::make_shared<const ::TimeTest>());
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<1>(cbs)(
      std::make_shared<const sensor_msgs::Image>());
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

  unit::test_unit::StereoMatch::PubSub pubsub(&logger,
      [&](auto input) {
        callback_called = true;
        return unit::test_unit::StereoMatch::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  auto cbs = CreateCallbacks(pubsub);

  auto a = std::make_shared<sensor_msgs::Image>();
  a->header.stamp.sec = 1;
  auto b = std::make_shared<sensor_msgs::Image>();
  b->header.stamp.sec = 2;

  std::get<0>(cbs)(b);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<1>(cbs)(a);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<1>(cbs)(b);
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

  unit::test_unit::ApproxTest::PubSub pubsub(&logger,
      [&](auto input) {
        callback_called = true;
        return unit::test_unit::ApproxTest::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  auto cbs = CreateCallbacks(pubsub);

  auto v_a = std::make_shared<::ExampleStampedVector>();
  v_a->set_time(0.95);
  auto v_b = std::make_shared<::ExampleStampedVector>();
  v_b->set_time(0.991);
  auto p_a = std::make_shared<sensor_msgs::PointCloud2>();
  p_a->header.stamp.sec = 1;

  std::get<0>(cbs)(v_a);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<1>(cbs)(p_a);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<0>(cbs)(v_b);
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

  unit::test_unit::TestEqualOptions::PubSub pubsub(&logger,
      [&](auto input) {
        callback_called = true;
        gotten_input = input;
        return unit::test_unit::TestEqualOptions::Output();
      },
      [&](auto output) { pub_callback_called = true; });

  auto cbs = CreateCallbacks(pubsub);
  auto a = std::make_shared<sensor_msgs::Image>();
  a->header.stamp.sec = 1;
  auto b = std::make_shared<sensor_msgs::Image>();
  b->header.stamp.sec = 2;

  std::get<0>(cbs)(b);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<1>(cbs)(a);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);
  std::get<1>(cbs)(b);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  std::get<3>(cbs)(std::make_shared<TestEmptyEvent>());
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

  std::get<2>(cbs)(std::make_shared<TestEmptyEvent>());
  std::get<2>(cbs)(std::make_shared<TestEmptyEvent>());

  std::get<3>(cbs)(std::make_shared<TestEmptyEvent>());
  std::get<3>(cbs)(std::make_shared<TestEmptyEvent>());
  std::get<3>(cbs)(std::make_shared<TestEmptyEvent>());
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  std::get<4>(cbs)(b);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  std::get<5>(cbs)(a);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  std::get<0>(cbs)(b);
  ASSERT_FALSE(callback_called);
  ASSERT_FALSE(pub_callback_called);

  std::get<1>(cbs)(b);
  ASSERT_TRUE(callback_called);
  ASSERT_TRUE(pub_callback_called);
  ASSERT_EQ(gotten_input.buffered_optional.size(), 2);
  ASSERT_EQ(gotten_input.buffered_non_optional.size(), 3);
  ASSERT_NE(gotten_input.optional_but_sync, nullptr);
  ASSERT_NE(gotten_input.optional, nullptr);
  ASSERT_NE(gotten_input.required_a, nullptr);
  ASSERT_NE(gotten_input.required_b, nullptr);
}

// Test that "magic" conversion from an inproc type to a message type works
TEST(TestUnitGeneration, TestInprocType) {  
  test_unit unit;

  unit.CreateTransportManager();
  unit.Initialize({});

  basis::core::transport::InprocTransport inproc_transport;
  auto reset = [&]() {
    unit.test_inproc_either_variant_executed = false;
    unit.test_inproc_variant_index = -1;
    unit.test_inproc_only_message_executed = false;
    unit.test_inproc_only_inproc_executed = false;
    unit.test_inproc_accumulated_input_executed = false;
  };

  //////////////////////////////////////////////////////////////////
  // Publishing from a dual publisher...
  auto trigger_pub = inproc_transport.Advertise<TimeTestInproc>("/inproc_test_trigger", nullptr);
  trigger_pub->Publish(std::make_shared<TimeTestInproc>(basis::core::MonotonicTime::Now()));
  unit.Update(nullptr, basis::core::Duration::FromSeconds(0.1));

  // dual subscriber gets it, uses the original type without conversion
  ASSERT_TRUE(unit.test_inproc_either_variant_executed);
  ASSERT_EQ(unit.test_inproc_variant_index, basis::MessageVariant::INPROC_TYPE_MESSAGE);
  // and both types of subscriber get it
  ASSERT_TRUE(unit.test_inproc_only_message_executed);
  ASSERT_TRUE(unit.test_inproc_only_inproc_executed);
  ASSERT_FALSE(unit.test_inproc_accumulated_input_executed);
  reset();

  //////////////////////////////////////////////////////////////////
  // Publishing from inproc only
  auto inproc_type_pub = inproc_transport.Advertise<TimeTestInproc>("/inproc_test", nullptr);
  inproc_type_pub->Publish(std::make_shared<TimeTestInproc>(basis::core::MonotonicTime::Now()));
  unit.Update(nullptr, basis::core::Duration::FromSeconds(0.1));

  // dual subscriber gets it, uses the original type without conversion
  ASSERT_TRUE(unit.test_inproc_either_variant_executed);
  ASSERT_EQ(unit.test_inproc_variant_index, basis::MessageVariant::INPROC_TYPE_MESSAGE);
  // Because this publisher doesn't know about the conversion, the regular typed subscriber doesn't get it
  ASSERT_FALSE(unit.test_inproc_only_message_executed);
  // but obviously the inproc one does
  ASSERT_TRUE(unit.test_inproc_only_inproc_executed);
  ASSERT_FALSE(unit.test_inproc_accumulated_input_executed);
  reset();

  //////////////////////////////////////////////////////////////////
  // Publishing from the protobuf type only
  auto type_pub = inproc_transport.Advertise<TimeTest>("/inproc_test", nullptr);
  type_pub->Publish(std::make_shared<TimeTest>());
  unit.Update(nullptr, basis::core::Duration::FromSeconds(0.1));

  // Same thing, but opposite
  ASSERT_TRUE(unit.test_inproc_either_variant_executed);
  ASSERT_EQ(unit.test_inproc_variant_index, basis::MessageVariant::TYPE_MESSAGE);
  ASSERT_TRUE(unit.test_inproc_only_message_executed);
  ASSERT_FALSE(unit.test_inproc_only_inproc_executed);
  ASSERT_FALSE(unit.test_inproc_accumulated_input_executed);
  reset();
  
  //////////////////////////////////////////////////////////////////
  // Excercise the accumulation code
  auto accumulate_trigger_pub = inproc_transport.Advertise<bool>("/inproc_test_accumulate_trigger", nullptr);
  accumulate_trigger_pub->Publish(std::make_shared<bool>());
  unit.Update(nullptr, basis::core::Duration::FromSeconds(0.1));
  ASSERT_TRUE(unit.test_inproc_accumulated_input_executed);
  // We've published three messages
  ASSERT_EQ(unit.test_inproc_accumulated_input.size(), 3);
  ASSERT_EQ(unit.test_inproc_accumulated_input[0].index(), basis::MessageVariant::INPROC_TYPE_MESSAGE);
  ASSERT_EQ(unit.test_inproc_accumulated_input[1].index(), basis::MessageVariant::INPROC_TYPE_MESSAGE);
  ASSERT_EQ(unit.test_inproc_accumulated_input[2].index(), basis::MessageVariant::TYPE_MESSAGE);
  

  //////////////////////////////////////////////////////////////////
  // Check that we don't convert unless we actually have a reason to.
  // This could be due to the recorder or due to a subscription to the other type, in this case we have neither
  auto dont_convert = std::make_shared<TimeTestInproc>();
  ASSERT_FALSE(dont_convert->ran_conversion_function);
  auto avoid_pointless_conversion_pub = inproc_transport.Advertise<TimeTestInproc>("/inproc_test_avoid_conversion", nullptr);
  avoid_pointless_conversion_pub->Publish(dont_convert);
  unit.Update(nullptr, basis::core::Duration::FromSeconds(0.1));
  // Ensure that we both ran the callback and _didnt_ run the conversion function
  ASSERT_TRUE(unit.test_inproc_avoid_pointless_conversion_executed);
  ASSERT_FALSE(dont_convert->ran_conversion_function);
  
  // For the future, it would be useful to excercise the TCP layer and recording bits of this
}