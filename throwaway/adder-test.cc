#include "throwaway/adder.h"
#include "gtest/gtest.h"

TEST(AdderTest, TestAddNumbersShouldReturnCorrectResults) { EXPECT_EQ(pl::AddNumbers(1, 2), 3); }
