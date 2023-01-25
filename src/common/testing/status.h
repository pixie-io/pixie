/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <utility>

#include <gmock/gmock.h>

#include "src/common/base/statusor.h"

namespace px {

inline ::testing::AssertionResult IsOK(const Status& status) {
  if (status.ok()) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << status.ToString();
}

}  // namespace px

#ifdef EXPECT_OK
// There is a conflicting name in status.h in protobuf.
#undef EXPECT_OK
#endif
#define EXPECT_OK(value) EXPECT_TRUE(IsOK(::px::StatusAdapter(value)))
#define EXPECT_NOT_OK(value) EXPECT_FALSE(IsOK(::px::StatusAdapter(value)))
#define ASSERT_OK(value) ASSERT_TRUE(IsOK(::px::StatusAdapter(value)))
#define ASSERT_NOT_OK(value) ASSERT_FALSE(IsOK(::px::StatusAdapter(value)))
#define EXPECT_PTR_VAL_EQ(val1, val2) EXPECT_EQ(*val1, *val2)

#define ASSERT_OK_AND_ASSIGN_IMPL(statusor, lhs, rexpr) \
  auto statusor = rexpr;                                \
  ASSERT_OK(statusor);                                  \
  lhs = std::move(statusor.ValueOrDie())

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSERT_OK_AND_ASSIGN_IMPL(PX_CONCAT_NAME(__status_or_value__, __COUNTER__), lhs, rexpr)

#define ASSERT_HAS_VALUE_AND_ASSIGN_IMPL(optional_var, lhs, rexpr) \
  auto optional_var = rexpr;                                       \
  ASSERT_TRUE(optional_var.has_value());                           \
  lhs = optional_var.value()

// For use with std::optional in testing.
#define ASSERT_HAS_VALUE_AND_ASSIGN(lhs, rexpr) \
  ASSERT_HAS_VALUE_AND_ASSIGN_IMPL(PX_CONCAT_NAME(__optional_var__, __COUNTER__), lhs, rexpr)

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

namespace px {
namespace testing {
namespace status {

template <typename ValueType>
struct IsOKAndHoldsMatcher {
  explicit IsOKAndHoldsMatcher(const ValueType& v) : value_(v) {}

  bool MatchAndExplain(const StatusOr<ValueType>& status_or,
                       ::testing::MatchResultListener* /*listener*/) const {
    return status_or.ok() && status_or.ValueOrDie() == value_;
  }

  void DescribeTo(::std::ostream* os) const { *os << "is OK and equals to: " << value_; }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is not OK or does not equal to: " << value_;
  }

  const ValueType& value_;
};

template <typename ValueType>
inline ::testing::PolymorphicMatcher<IsOKAndHoldsMatcher<ValueType>> IsOKAndHolds(
    const ValueType& v) {
  return ::testing::MakePolymorphicMatcher(IsOKAndHoldsMatcher(v));
}

template <typename TMessageMatcherType>
auto StatusIs(px::statuspb::Code code, const TMessageMatcherType& msg_matcher) {
  return ::testing::AllOf(::testing::Property(&Status::code, ::testing::Eq(code)),
                          ::testing::Property(&Status::msg, msg_matcher));
}

template <typename TMessageMatcherType>
auto StatusMsgIs(const TMessageMatcherType& msg_matcher) {
  return ::testing::Property(&Status::msg, msg_matcher);
}

}  // namespace status
}  // namespace testing
}  // namespace px
