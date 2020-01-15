#pragma once

#include <gtest/gtest.h>

#include "src/common/base/status.h"

namespace pl {

inline ::testing::AssertionResult IsOK(const Status& status) {
  if (status.ok()) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << status.ToString();
}

}  // namespace pl

#define PL_RETURN_IF_ERROR_IMPL(__status_name__, __status) \
  do {                                                     \
    const auto& __status_name__ = (__status);              \
    if (!__status_name__.ok()) {                           \
      return StatusAdapter(__status_name__);               \
    }                                                      \
  } while (false)

// Early-returns the status if it is in error; otherwise, proceeds.
// The argument expression is guaranteed to be evaluated exactly once.
#define PL_RETURN_IF_ERROR(__status) PL_RETURN_IF_ERROR_IMPL(PL_UNIQUE_NAME(__status__), __status)

#ifdef EXPECT_OK
// There is a conflicting name in status.h in protobuf.
#undef EXPECT_OK
#endif
// TODO(yzhao): Consider rename to PL_EXPECT_OK.
#define EXPECT_OK(value) EXPECT_TRUE(IsOK(::pl::StatusAdapter(value)))
#define EXPECT_NOT_OK(value) EXPECT_FALSE(IsOK(::pl::StatusAdapter(value)))
#define ASSERT_OK(value) ASSERT_TRUE(IsOK(::pl::StatusAdapter(value)))
#define ASSERT_NOT_OK(value) ASSERT_FALSE(IsOK(::pl::StatusAdapter(value)))

#define EXPECT_OK_AND_EQ(status, value)             \
  {                                                 \
    EXPECT_OK(status);                              \
    if (status.ok()) {                              \
      EXPECT_EQ(status.ConsumeValueOrDie(), value); \
    }                                               \
  }
#define ASSERT_OK_AND_EQ(status, value)           \
  {                                               \
    ASSERT_OK(status);                            \
    ASSERT_EQ(status.ConsumeValueOrDie(), value); \
  }
