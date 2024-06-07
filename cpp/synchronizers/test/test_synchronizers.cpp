#include <basis/synchronizers/all.h>

#include <gtest/gtest.h>

TEST(TestSyncAll, BasicTest) {
  std::atomic<bool> was_called{false};
  std::function test_cb = [&](std::shared_ptr<char> a, std::shared_ptr<char> b) {
    was_called = true;
    ASSERT_EQ(*a, 'a');
    ASSERT_EQ(*b, 'b');
  };

  basis::synchronizers::All<char, char> test_all(test_cb);

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

  basis::synchronizers::All<char, char> test_all(
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

  basis::synchronizers::All<char, char> test_all(test_cb, {}, {.is_cached = true});

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
