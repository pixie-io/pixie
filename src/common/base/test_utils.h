#pragma once

#include <gtest/gtest.h>

#include <utility>

#include "src/common/base/status.h"

namespace pl {

inline ::testing::AssertionResult IsOK(const Status& status) {
  if (status.ok()) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << status.ToString();
}

}  // namespace pl

#ifdef EXPECT_OK
// There is a conflicting name in status.h in protobuf.
#undef EXPECT_OK
#endif
// TODO(yzhao): Consider rename to PL_EXPECT_OK.
#define EXPECT_OK(value) EXPECT_TRUE(IsOK(::pl::StatusAdapter(value)))
#define EXPECT_NOT_OK(value) EXPECT_FALSE(IsOK(::pl::StatusAdapter(value)))
#define ASSERT_OK(value) ASSERT_TRUE(IsOK(::pl::StatusAdapter(value)))
#define ASSERT_NOT_OK(value) ASSERT_FALSE(IsOK(::pl::StatusAdapter(value)))
#define EXPECT_PTR_VAL_EQ(val1, val2) EXPECT_EQ(*val1, *val2)

#define ASSERT_OK_AND_ASSIGN_IMPL(statusor, lhs, rexpr) \
  auto statusor = rexpr;                                \
  ASSERT_OK(statusor);                                  \
  lhs = std::move(statusor.ValueOrDie())

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSERT_OK_AND_ASSIGN_IMPL(PL_CONCAT_NAME(__status_or_value__, __COUNTER__), lhs, rexpr)

#define ASSERT_HAS_VALUE_AND_ASSIGN_IMPL(optional_var, lhs, rexpr) \
  auto optional_var = rexpr;                                       \
  ASSERT_TRUE(optional_var.has_value());                           \
  lhs = optional_var.value()

// For use with std::optional in testing.
#define ASSERT_HAS_VALUE_AND_ASSIGN(lhs, rexpr) \
  ASSERT_HAS_VALUE_AND_ASSIGN_IMPL(PL_CONCAT_NAME(__optional_var__, __COUNTER__), lhs, rexpr)

#define EXPECT_OK_AND(expect_fn, expr, value) \
  {                                           \
    auto&& __s__ = expr;                      \
    EXPECT_OK(__s__);                         \
    if (__s__.ok()) {                         \
      expect_fn(__s__.ValueOrDie(), value);   \
    }                                         \
  }
#define ASSERT_OK_AND(assert_fn, expr, value) \
  {                                           \
    auto&& __s__ = expr;                      \
    ASSERT_OK(__s__);                         \
    assert_fn(__s__.ValueOrDie(), value);     \
  }

#define EXPECT_OK_AND_EQ(expr, value) EXPECT_OK_AND(EXPECT_EQ, expr, value)
#define EXPECT_OK_AND_NE(expr, value) EXPECT_OK_AND(EXPECT_NE, expr, value)
#define EXPECT_OK_AND_LE(expr, value) EXPECT_OK_AND(EXPECT_LE, expr, value)
#define EXPECT_OK_AND_LT(expr, value) EXPECT_OK_AND(EXPECT_LT, expr, value)
#define EXPECT_OK_AND_GE(expr, value) EXPECT_OK_AND(EXPECT_GE, expr, value)
#define EXPECT_OK_AND_GT(expr, value) EXPECT_OK_AND(EXPECT_GT, expr, value)
#define EXPECT_OK_AND_THAT(expr, value) EXPECT_OK_AND(EXPECT_THAT, expr, value)
#define EXPECT_OK_AND_PTR_VAL_EQ(expr, value) EXPECT_OK_AND(EXPECT_PTR_VAL_EQ, expr, value)

#define ASSERT_OK_AND_EQ(expr, value) ASSERT_OK_AND(ASSERT_EQ, expr, value)
#define ASSERT_OK_AND_NE(expr, value) ASSERT_OK_AND(ASSERT_NE, expr, value)
#define ASSERT_OK_AND_LE(expr, value) ASSERT_OK_AND(ASSERT_LE, expr, value)
#define ASSERT_OK_AND_LT(expr, value) ASSERT_OK_AND(ASSERT_LT, expr, value)
#define ASSERT_OK_AND_GE(expr, value) ASSERT_OK_AND(ASSERT_GE, expr, value)
#define ASSERT_OK_AND_GT(expr, value) ASSERT_OK_AND(ASSERT_GT, expr, value)
#define ASSERT_OK_AND_THAT(expr, value) ASSERT_OK_AND(ASSERT_THAT, expr, value)
