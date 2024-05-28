#include <basis/core/coordinator.h>
#include <gtest/gtest.h>

TEST(TestCoordinator, BasicTest) {
    basis::core::transport::Coordinator coordinator = *basis::core::transport::Coordinator::Create();

    coordinator.Update();
}