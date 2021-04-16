#pragma once

#include <gmock/gmock.h>

#include "src/common/base/statusor.h"

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

}  // namespace status
}  // namespace testing
}  // namespace px
