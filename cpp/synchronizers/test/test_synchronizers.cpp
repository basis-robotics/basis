#include <basis/synchronizers/all.h>
#include <basis/synchronizers/field.h>

#include <gtest/gtest.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <test.pb.h>
#pragma clang diagnostic pop

TEST(TestSyncAll, BasicTest) {
  std::atomic<bool> was_called{false};
  std::function test_cb = [&](std::shared_ptr<char> a, std::shared_ptr<char> b) {
    was_called = true;
    ASSERT_EQ(*a, 'a');
    ASSERT_EQ(*b, 'b');
  };

  basis::synchronizers::All<std::shared_ptr<char>, std::shared_ptr<char>> test_all(test_cb);

  auto a = std::make_shared<char>('a');
  auto b = std::make_shared<char>('b');

  // Shouldn't be called
  test_all.OnMessage<0>(a);
  ASSERT_FALSE(was_called);
  // Shouldn't be called
  test_all.OnMessage<0>(a);
  ASSERT_FALSE(was_called);

  test_all.OnMessage<1>(b);
  ASSERT_TRUE(was_called);
  was_called = false;

  test_all.OnMessage<1>(b);
  ASSERT_FALSE(was_called);

  test_all.OnMessage<0>(a);
  ASSERT_TRUE(was_called);
}

TEST(TestSyncAll, TestOptional) {
  std::atomic<bool> was_called{false};
  std::shared_ptr<char> recvd_a;
  std::shared_ptr<char> recvd_b;

  basis::synchronizers::All<std::shared_ptr<char>, std::shared_ptr<char>> test_all(
      [&](auto a, auto b) {
        was_called = true;
        recvd_a = a;
        recvd_b = b;
      },
      {}, {.is_optional = true});

  auto a = std::make_shared<char>('a');
  auto b = std::make_shared<char>('b');

  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b, nullptr);

  test_all.OnMessage<0>(a);
  ASSERT_EQ(*recvd_a, 'a');
  ASSERT_EQ(recvd_b, nullptr);
  recvd_a = nullptr;

  test_all.OnMessage<1>(b);
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b, nullptr);

  test_all.OnMessage<0>(a);
  ASSERT_EQ(*recvd_a, 'a');
  ASSERT_EQ(*recvd_b, 'b');
}

TEST(TestSyncAll, TestCached) {
  std::atomic<bool> was_called{false};
  std::function test_cb = [&](std::shared_ptr<char> a, std::shared_ptr<char> b) {
    was_called = true;
    ASSERT_EQ(*a, 'a');
    ASSERT_EQ(*b, 'b');
  };

  basis::synchronizers::All<std::shared_ptr<char>, std::shared_ptr<char>> test_all(test_cb, {}, {.is_cached = true});

  auto a = std::make_shared<char>('a');
  auto b = std::make_shared<char>('b');

  ASSERT_EQ(was_called, false);

  test_all.OnMessage<0>(a);
  ASSERT_EQ(was_called, false);

  test_all.OnMessage<1>(b);
  ASSERT_EQ(was_called, true);
  was_called = false;

  test_all.OnMessage<0>(a);
  ASSERT_EQ(was_called, true);
  was_called = false;

  test_all.OnMessage<1>(b);
  ASSERT_EQ(was_called, false);
  test_all.OnMessage<0>(a);
  ASSERT_EQ(was_called, true);
}

TEST(TestSyncAll, TestContainer) {
  std::atomic<bool> was_called{false};

  std::shared_ptr<char> recvd_a;
  std::vector<std::shared_ptr<char>> recvd_b;

  basis::synchronizers::All<std::shared_ptr<char>, std::vector<std::shared_ptr<char>>> test_all([&](auto a, auto b) {
    was_called = true;
    recvd_a = a;
    recvd_b = b;
  });

  auto a = std::make_shared<char>('a');
  auto b1 = std::make_shared<char>('1');
  auto b2 = std::make_shared<char>('2');
  auto b3 = std::make_shared<char>('3');

  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);

  test_all.OnMessage<0>(a);
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);

  test_all.OnMessage<1>(b1);
  ASSERT_EQ(recvd_a, a);
  ASSERT_EQ(recvd_b.size(), 1);
  ASSERT_EQ(recvd_b, decltype(recvd_b)({b1}));

  recvd_a.reset();
  recvd_b.clear();

  test_all.OnMessage<1>(b1);
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);
  test_all.OnMessage<1>(b2);
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);
  test_all.OnMessage<1>(b3);
  ASSERT_EQ(recvd_a, nullptr);
  ASSERT_EQ(recvd_b.size(), 0);

  test_all.OnMessage<0>(a);
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
  ASSERT_FALSE(test.OnMessage<0>(std::make_shared<Foo>(2)));
  // [1], [2], []
  ASSERT_FALSE(test.OnMessage<1>(produce_proto(1)));
  // [1], [2], [X]
  ASSERT_FALSE(test.OnMessage<2>(unsynced));
  // [1, 2], [2], [X] (sync on 2)
  ASSERT_TRUE(test.OnMessage<1>(produce_proto(2)));
  // [], [], []

  // Check that when we sync, we leave data in the buffer for later
  // [], [], [X]
  ASSERT_FALSE(test.OnMessage<2>(unsynced));
  // [3], [], [X]
  ASSERT_FALSE(test.OnMessage<1>(produce_proto(3)));
  // [3, 4], [], [X]
  ASSERT_FALSE(test.OnMessage<1>(produce_proto(4)));
  // [3, 4], [3], [X] (sync on 3)
  ASSERT_TRUE(test.OnMessage<0>(std::make_shared<Foo>(3)));
  // [4], [], []

  // [4], [], [X]
  ASSERT_FALSE(test.OnMessage<2>(unsynced));
  // [4], [4], [X] (sync on 4)
  ASSERT_TRUE(test.OnMessage<0>(std::make_shared<Foo>(4)));
  // [], [], []

  // Test that when we sync, we still wait for unsynced messages

  // [5], [5], [] (sync on 5, but no output)
  ASSERT_FALSE(test.OnMessage<0>(std::make_shared<Foo>(5)));
  ASSERT_FALSE(test.OnMessage<1>(produce_proto(5)));
  // [5], [5], [X]
  ASSERT_TRUE(test.OnMessage<2>(unsynced));
}
