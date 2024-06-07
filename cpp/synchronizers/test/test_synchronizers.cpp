#include <basis/synchronizers/all.h>

#include <gtest/gtest.h>

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