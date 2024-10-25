#include <basis/synchronizers/all.h>
#include <basis/synchronizers/field.h>

#include <gtest/gtest.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <test.pb.h>
#pragma clang diagnostic pop

TEST(TestSyncAll, BasicTest) {

  basis::synchronizers::All<std::shared_ptr<char>, std::shared_ptr<char>> test_all;

  std::atomic<bool> was_called{false};
  std::function test_cb = [&](const std::optional<decltype(test_all)::MessageSumType> &maybe_tuple) {
    if (maybe_tuple) {
      was_called = true;
      auto &[a, b] = *maybe_tuple;
      ASSERT_EQ(*a, 'a');
      ASSERT_EQ(*b, 'b');
    }
  };

  auto a = std::make_shared<char>('a');
  auto b = std::make_shared<char>('b');

  // Shouldn't be called
  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_FALSE(was_called);
  // Shouldn't be called
  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_FALSE(was_called);

  test_all.OnMessage<1>(b);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_TRUE(was_called);
  was_called = false;

  test_all.OnMessage<1>(b);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_FALSE(was_called);

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_TRUE(was_called);
}

TEST(TestSyncAll, TestOptional) {
  std::atomic<bool> was_called{false};
  std::shared_ptr<char> recvd_a;
  std::shared_ptr<char> recvd_b;

  basis::synchronizers::All<std::shared_ptr<char>, std::shared_ptr<char>> test_all({}, {.is_optional = true});

  std::function test_cb = [&](const std::optional<decltype(test_all)::MessageSumType> &maybe_tuple) {
    if (maybe_tuple) {
      was_called = true;
      auto [a, b] = *maybe_tuple;
      recvd_a = a;
      recvd_b = b;
    }
  };

  auto a = std::make_shared<char>('a');
  auto b = std::make_shared<char>('b');

  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b, nullptr);

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(*recvd_a, 'a');
  ASSERT_EQ(recvd_b, nullptr);
  recvd_a = nullptr;

  test_all.OnMessage<1>(b);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b, nullptr);

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(*recvd_a, 'a');
  ASSERT_EQ(*recvd_b, 'b');
}

TEST(TestSyncAll, TestCached) {
  std::atomic<bool> was_called{false};

  basis::synchronizers::All<std::shared_ptr<char>, std::shared_ptr<char>> test_all({}, {.is_cached = true});
  std::function test_cb = [&](const std::optional<decltype(test_all)::MessageSumType> &maybe_tuple) {
    if (maybe_tuple) {
      was_called = true;
      auto &[a, b] = *maybe_tuple;
      ASSERT_EQ(*a, 'a');
      ASSERT_EQ(*b, 'b');
    }
  };
  auto a = std::make_shared<char>('a');
  auto b = std::make_shared<char>('b');

  ASSERT_EQ(was_called, false);

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(was_called, false);

  test_all.OnMessage<1>(b);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(was_called, true);
  was_called = false;

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(was_called, true);
  was_called = false;

  test_all.OnMessage<1>(b);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(was_called, false);
  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(was_called, true);
}

TEST(TestSyncAll, TestContainer) {
  std::atomic<bool> was_called{false};

  std::shared_ptr<char> recvd_a;
  std::vector<std::shared_ptr<char>> recvd_b;

  basis::synchronizers::All<std::shared_ptr<char>, std::vector<std::shared_ptr<char>>> test_all;
  std::function test_cb = [&](const std::optional<decltype(test_all)::MessageSumType> &maybe_tuple) {
    if (maybe_tuple) {
      auto &[a, b] = *maybe_tuple;
      was_called = true;
      recvd_a = a;
      recvd_b = b;
    }
  };

  auto a = std::make_shared<char>('a');
  auto b1 = std::make_shared<char>('1');
  auto b2 = std::make_shared<char>('2');
  auto b3 = std::make_shared<char>('3');

  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);

  test_all.OnMessage<1>(b1);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, a);
  ASSERT_EQ(recvd_b.size(), 1);
  ASSERT_EQ(recvd_b, decltype(recvd_b)({b1}));

  recvd_a.reset();
  recvd_b.clear();

  test_all.OnMessage<1>(b1);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);
  test_all.OnMessage<1>(b2);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);
  test_all.OnMessage<1>(b3);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);

  test_all.OnMessage<0>(a);
  test_cb(test_all.ConsumeIfReady());
  ASSERT_EQ(recvd_a, a);

  ASSERT_EQ(recvd_b, decltype(recvd_b)({b1, b2, b3}));
}

struct Foo {
  uint32_t foo;
};

struct Unsynced {
  uint32_t unsynced;
};

uint32_t GetMember(const OuterSyncTestStruct *outer) { return outer->header().stamp(); }

TEST(TestSyncField, BasicTest) {
  auto produce_proto = [](uint32_t stamp) {
    auto proto = std::make_shared<OuterSyncTestStruct>();
    proto->mutable_header()->set_stamp(stamp);
    return proto;
  };

  basis::synchronizers::FieldSyncEqual<
      basis::synchronizers::Field<std::shared_ptr<const Foo>, &Foo::foo>,
      basis::synchronizers::Field<std::shared_ptr<const OuterSyncTestStruct>,
                                  [](const OuterSyncTestStruct *outer) { return outer->header().stamp(); }>,
      basis::synchronizers::Field<std::vector<std::shared_ptr<const Unsynced>>, nullptr>>
      test;

  auto unsynced = std::make_shared<Unsynced>(0xFF);

  // Check that we sync at all
  // [1], [], []
  test.OnMessage<0>(std::make_shared<Foo>(2));
  ASSERT_FALSE(test.ConsumeIfReady());
  // [1], [2], []
  test.OnMessage<1>(produce_proto(1));
  ASSERT_FALSE(test.ConsumeIfReady());
  // [1], [2], [X]
  test.OnMessage<2>(unsynced);
  ASSERT_FALSE(test.ConsumeIfReady());
  // [1, 2], [2], [X] (sync on 2)
  test.OnMessage<1>(produce_proto(2));
  ASSERT_TRUE(test.ConsumeIfReady());
  // [], [], []

  // Check that when we sync, we leave data in the buffer for later
  // [], [], [X]
  test.OnMessage<2>(unsynced);
  ASSERT_FALSE(test.ConsumeIfReady());
  // [3], [], [X]
  test.OnMessage<1>(produce_proto(3));
  ASSERT_FALSE(test.ConsumeIfReady());
  // [3, 4], [], [X]
  test.OnMessage<1>(produce_proto(4));
  ASSERT_FALSE(test.ConsumeIfReady());
  // [3, 4], [3], [X] (sync on 3)
  test.OnMessage<0>(std::make_shared<Foo>(3));
  ASSERT_TRUE(test.ConsumeIfReady());
  // [4], [], []

  // [4], [], [X]
  test.OnMessage<2>(unsynced);
  ASSERT_FALSE(test.ConsumeIfReady());
  // [4], [4], [X] (sync on 4)
  test.OnMessage<0>(std::make_shared<Foo>(4));
  ASSERT_TRUE(test.ConsumeIfReady());
  // [], [], []

  // Test that when we sync, we still wait for unsynced messages

  // [5], [5], [] (sync on 5, but no output)
  test.OnMessage<0>(std::make_shared<Foo>(5));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(produce_proto(5));
  ASSERT_FALSE(test.ConsumeIfReady());
  // [5], [5], [X]
  test.OnMessage<2>(unsynced);
  ASSERT_TRUE(test.ConsumeIfReady());
}

struct ApproxTest {
  double f = 0.0;
};

TEST(TestSyncField, TestApproximate) {
  basis::synchronizers::FieldSyncApproximatelyEqual<
      0.01, basis::synchronizers::Field<std::shared_ptr<const ApproxTest>, &ApproxTest::f>,
      basis::synchronizers::Field<std::shared_ptr<const ApproxTest>, &ApproxTest::f>,
      basis::synchronizers::Field<std::vector<std::shared_ptr<const Unsynced>>, nullptr>>
      test({}, {}, {.is_cached = true});

  // Test the same number, but no third memeber
  test.OnMessage<0>(std::make_shared<ApproxTest>(0.0));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<ApproxTest>(0.0));
  ASSERT_FALSE(test.ConsumeIfReady());
  auto unsynced = std::make_shared<Unsynced>(0xFF);
  test.OnMessage<2>(unsynced);
  ASSERT_TRUE(test.ConsumeIfReady());
  // Shouldn't need unsynced after this as we are cached

  // Test the same number, again
  test.OnMessage<0>(std::make_shared<ApproxTest>(1.0));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<ApproxTest>(1.0));
  ASSERT_TRUE(test.ConsumeIfReady());

  // Test numbers that are too far apart
  test.OnMessage<0>(std::make_shared<ApproxTest>(2.0));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<ApproxTest>(3.0));
  ASSERT_FALSE(test.ConsumeIfReady());

  // Test numbers that are similar
  test.OnMessage<0>(std::make_shared<ApproxTest>(3.001));
  ASSERT_TRUE(test.ConsumeIfReady());

  // Test numbers that aren't quite similar enough
  test.OnMessage<0>(std::make_shared<ApproxTest>(4.0));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<ApproxTest>(4.011));
  ASSERT_FALSE(test.ConsumeIfReady());
}

struct TypeConversionString {
  std::string s;
};

struct TypeConversionInt {
  int64_t i;
};

TEST(TestSyncField, TestTypeConversion) {
  basis::synchronizers::FieldSyncEqual<
      basis::synchronizers::Field<std::shared_ptr<const TypeConversionString>, &TypeConversionString::s>,
      basis::synchronizers::Field<std::shared_ptr<const TypeConversionInt>,
                                  [](const TypeConversionInt *i) { return std::to_string(i->i); }>>
      test;

  test.OnMessage<0>(std::make_shared<TypeConversionString>("foobar"));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<TypeConversionInt>(1));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<0>(std::make_shared<TypeConversionString>("1"));
  ASSERT_TRUE(test.ConsumeIfReady());
  test.OnMessage<0>(std::make_shared<TypeConversionString>("33"));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<0>(std::make_shared<TypeConversionString>("42"));
  ASSERT_FALSE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<TypeConversionInt>(33));
  ASSERT_TRUE(test.ConsumeIfReady());
  test.OnMessage<1>(std::make_shared<TypeConversionInt>(42));
  ASSERT_TRUE(test.ConsumeIfReady());
}