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
#include "src/common/testing/line_diff.h"

namespace px {
namespace testing {
namespace proto {

struct ProtoMatcher {
  ProtoMatcher(google::protobuf::util::MessageDifferencer* differencer, std::string text_pb)
      : expected_text_pb_(std::move(text_pb)), differencer_(differencer) {}

  template <typename PBType>
  bool MatchAndExplain(const PBType& pb, ::testing::MatchResultListener* listener) const {
    std::unique_ptr<PBType> expected_pb(pb.New());
    if (!google::protobuf::TextFormat::ParseFromString(expected_text_pb_, expected_pb.get())) {
      (*listener) << "The input cannot be parsed as protobuf, got:\n" << pb.DebugString();
      return false;
    }
    // diff_report were not used, instead we use line-based text diffing below.
    std::string diff_report;
    differencer_->ReportDifferencesToString(&diff_report);

    if (optional_scope_.has_value()) {
      differencer_->set_scope(optional_scope_.value());
    }

    if (!differencer_->Compare(*expected_pb, pb)) {
      std::string pb_text;
      google::protobuf::TextFormat::PrintToString(pb, &pb_text);
      (*listener) << "diff:\n"
                  << DiffLines(pb_text, expected_text_pb_, DiffPolicy::kIgnoreBlankLines);
      return false;
    }
    return true;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "equals to text probobuf: " << expected_text_pb_;
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal to text protobuf: " << expected_text_pb_;
  }

  std::string expected_text_pb_;
  google::protobuf::util::MessageDifferencer* differencer_;
  std::optional<google::protobuf::util::MessageDifferencer::Scope> optional_scope_;
};

struct EqualsProtoMatcher : public ProtoMatcher {
  explicit EqualsProtoMatcher(std::string text_pb) : ProtoMatcher(nullptr, std::move(text_pb)) {
    vanilla_differencer_ = std::make_shared<google::protobuf::util::MessageDifferencer>();
    differencer_ = vanilla_differencer_.get();
  }
  // NOTE: using a shared_ptr here instead of as a regular object because MessageDifferencer
  // does not have a copy constructor and MakePolymorphicMatccher copies the passed in
  // Matcher object (which then copies this member).
  std::shared_ptr<google::protobuf::util::MessageDifferencer> vanilla_differencer_;
};

struct PartiallyEqualsProtoMatcher : public ProtoMatcher {
  PartiallyEqualsProtoMatcher(google::protobuf::util::MessageDifferencer* differencer,
                              std::string text_pb)
      : ProtoMatcher(differencer, std::move(text_pb)) {
    optional_scope_ = google::protobuf::util::MessageDifferencer::Scope::PARTIAL;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "partially equals to text probobuf: " << expected_text_pb_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "does not partially equal to text protobuf: " << expected_text_pb_;
  }
};

inline ::testing::PolymorphicMatcher<EqualsProtoMatcher> EqualsProto(std::string text_pb) {
  return ::testing::MakePolymorphicMatcher(EqualsProtoMatcher(std::move(text_pb)));
}

inline ::testing::PolymorphicMatcher<PartiallyEqualsProtoMatcher> Partially(
    const ::testing::PolymorphicMatcher<EqualsProtoMatcher>& matcher) {
  return ::testing::MakePolymorphicMatcher(
      PartiallyEqualsProtoMatcher(matcher.impl().differencer_, matcher.impl().expected_text_pb_));
}

inline ::testing::PolymorphicMatcher<PartiallyEqualsProtoMatcher> Partially(
    const ::testing::PolymorphicMatcher<ProtoMatcher>& matcher) {
  return ::testing::MakePolymorphicMatcher(
      PartiallyEqualsProtoMatcher(matcher.impl().differencer_, matcher.impl().expected_text_pb_));
}

inline ::testing::PolymorphicMatcher<ProtoMatcher> WithDifferencer(
    google::protobuf::util::MessageDifferencer* differencer,
    const ::testing::PolymorphicMatcher<EqualsProtoMatcher>& matcher) {
  return ::testing::MakePolymorphicMatcher(
      ProtoMatcher(differencer, matcher.impl().expected_text_pb_));
}

}  // namespace proto
}  // namespace testing
}  // namespace px
