#pragma once

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "src/common/base/logging.h"

namespace pl {
namespace testing {
namespace proto {

struct EqualsProtoMatcher {
  explicit EqualsProtoMatcher(std::string text_pb) : expected_text_pb_(std::move(text_pb)) {}

  template <typename PBType>
  bool MatchAndExplain(const PBType& pb, ::testing::MatchResultListener* listener) const {
    std::unique_ptr<PBType> expected_pb(pb.New());
    if (!google::protobuf::TextFormat::ParseFromString(expected_text_pb_, expected_pb.get())) {
      (*listener) << "The input cannot be parsed as protobuf!";
      return false;
    }
    google::protobuf::util::MessageDifferencer differencer;
    std::string diff_report;
    differencer.ReportDifferencesToString(&diff_report);
    if (optional_scope_.has_value()) {
      differencer.set_scope(optional_scope_.value());
    }
    if (!differencer.Compare(*expected_pb, pb)) {
      (*listener) << diff_report << "got:\n" << pb.DebugString();
      return false;
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "equals to text probobuf: " << expected_text_pb_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal to text protobuf: " << expected_text_pb_;
  }

  std::string expected_text_pb_;
  std::optional<google::protobuf::util::MessageDifferencer::Scope> optional_scope_;
};

struct PartiallyEqualsProtoMatcher : public EqualsProtoMatcher {
  explicit PartiallyEqualsProtoMatcher(std::string text_pb)
      : EqualsProtoMatcher(std::move(text_pb)) {
    optional_scope_ = google::protobuf::util::MessageDifferencer::Scope::PARTIAL;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "partially equals to text probobuf: " << expected_text_pb_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not partially equal to text protobuf: " << expected_text_pb_;
  }
};

inline ::testing::PolymorphicMatcher<EqualsProtoMatcher> EqualsProto(std::string text_pb) {
  return ::testing::MakePolymorphicMatcher(EqualsProtoMatcher(std::move(text_pb)));
}

inline ::testing::PolymorphicMatcher<PartiallyEqualsProtoMatcher> Partially(
    const ::testing::PolymorphicMatcher<EqualsProtoMatcher>& matcher) {
  return ::testing::MakePolymorphicMatcher(
      PartiallyEqualsProtoMatcher(matcher.impl().expected_text_pb_));
}

}  // namespace proto
}  // namespace testing
}  // namespace pl
