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

#include "experimental/protobufs/aliasing.h"

#include <google/protobuf/util/message_differencer.h>

#include <cmath>
#include <vector>

#include "experimental/protobufs/proto/alias.pb.h"
#include "src/common/base/logging.h"

namespace experimental {
namespace protobufs {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorProto;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::FieldDescriptorProto;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;
using ::google::protobuf::RepeatedPtrField;
using ::google::protobuf::util::MessageDifferencer;

int IsMoreSpecialized(const Message& lhs, const Message& rhs) {
  // Returns the descriptors of the fields have been set in the message.
  auto get_set_field_descs = [](const Message& message) -> std::vector<const FieldDescriptor*> {
    const Reflection* refl = message.GetReflection();
    std::vector<const FieldDescriptor*> res;
    refl->ListFields(message, &res);
    return res;
  };

  auto lhs_field_descs = get_set_field_descs(lhs);
  DCHECK(!lhs_field_descs.empty());

  auto rhs_field_descs = get_set_field_descs(rhs);
  DCHECK(!rhs_field_descs.empty());

  auto comp_by_field_number = [](const FieldDescriptor* lhs, const FieldDescriptor* rhs) -> bool {
    return lhs->number() < rhs->number();
  };
  std::sort(lhs_field_descs.begin(), lhs_field_descs.end(), comp_by_field_number);
  std::sort(rhs_field_descs.begin(), rhs_field_descs.end(), comp_by_field_number);

  for (size_t i = 0; i < lhs_field_descs.size(); ++i) {
    auto* lhs_desc = lhs_field_descs[i];
    auto* rhs_desc = rhs_field_descs[i];
    DCHECK(lhs_desc->number() == rhs_desc->number());
    if (lhs_desc->type() == rhs_desc->type()) {
      continue;
    }
    if (lhs_desc->type() == FieldDescriptor::TYPE_MESSAGE &&
        rhs_desc->type() != FieldDescriptor::TYPE_MESSAGE) {
      return -1;
    }
    if (lhs_desc->type() != FieldDescriptor::TYPE_MESSAGE &&
        rhs_desc->type() == FieldDescriptor::TYPE_MESSAGE) {
      return 1;
    }
  }
  return 0;
}

namespace {

// Returns the descriptors of the fields sorted in ascending order of field number.
std::vector<const FieldDescriptorProto*> GetSortedFieldDescs(const DescriptorProto& desc) {
  std::vector<const FieldDescriptorProto*> res;
  res.reserve(desc.field_size());
  for (int i = 0; i < desc.field_size(); ++i) {
    res.push_back(&desc.field(i));
  }
  std::sort(res.begin(), res.end(),
            [](const FieldDescriptorProto* lhs, const FieldDescriptorProto* rhs) {
              return lhs->number() < rhs->number();
            });
  return res;
}

const std::set<FieldDescriptorProto::Type> kVarintTypeSet = {
    FieldDescriptorProto::TYPE_INT32,  FieldDescriptorProto::TYPE_INT64,
    FieldDescriptorProto::TYPE_UINT32, FieldDescriptorProto::TYPE_UINT64,
    FieldDescriptorProto::TYPE_SINT32, FieldDescriptorProto::TYPE_SINT64,
    FieldDescriptorProto::TYPE_BOOL,   FieldDescriptorProto::TYPE_ENUM,
};

const std::set<FieldDescriptorProto::Type> kFixed32BitTypeSet = {
    FieldDescriptorProto::TYPE_FIXED32,
    FieldDescriptorProto::TYPE_SFIXED32,
    FieldDescriptorProto::TYPE_FLOAT,
};

const std::set<FieldDescriptorProto::Type> kFixed64BitTypeSet = {
    FieldDescriptorProto::TYPE_FIXED64,
    FieldDescriptorProto::TYPE_SFIXED64,
    FieldDescriptorProto::TYPE_DOUBLE,
};

const std::set<FieldDescriptorProto::Type> kStringAndBytes = {
    FieldDescriptorProto::TYPE_STRING,
    FieldDescriptorProto::TYPE_BYTES,
};

std::set<FieldDescriptorProto::Type> NumericTypeSet() {
  std::set<FieldDescriptorProto::Type> res;
  res.insert(kVarintTypeSet.begin(), kVarintTypeSet.end());
  res.insert(kFixed32BitTypeSet.begin(), kFixed32BitTypeSet.end());
  res.insert(kFixed64BitTypeSet.begin(), kFixed64BitTypeSet.end());
  return res;
}

const std::set<FieldDescriptorProto::Type> kNumericTypeSet = NumericTypeSet();

bool Contains(std::set<FieldDescriptorProto::Type> type_set, FieldDescriptorProto::Type type) {
  return type_set.find(type) != type_set.end();
}

AliasingFieldPair CreateAliasingFieldPair(const FieldDescriptorProto* first,
                                          const FieldDescriptorProto* second,
                                          AliasingFieldPair::Direction direction) {
  AliasingFieldPair res;
  *res.mutable_first() = *first;
  *res.mutable_second() = *second;
  res.set_direction(direction);
  return res;
}

}  // namespace

std::vector<AliasingFieldPair> GetAliasingFieldPairs(const DescriptorProto& first,
                                                     const DescriptorProto& second) {
  auto first_field_descs = GetSortedFieldDescs(first);
  auto second_field_descs = GetSortedFieldDescs(second);

  std::vector<AliasingFieldPair> res;
  for (size_t i = 0, j = 0; i < first_field_descs.size() && j < second_field_descs.size();) {
    // Save typing
    const auto* field1 = first_field_descs[i];
    const auto* field2 = second_field_descs[j];

    const auto label_repeated = FieldDescriptorProto::LABEL_REPEATED;

    const auto number1 = field1->type();
    const auto type1 = field1->type();
    const auto label1 = field1->label();
    const auto name1 = field1->type_name();

    const auto number2 = field2->type();
    const auto type2 = field2->type();
    const auto label2 = field2->label();
    const auto name2 = field2->type_name();

    if (number1 < number2) {
      ++i;
    } else if (number1 > number2) {
      ++j;
    } else {
      ++i;
      ++j;
      // The following is a list of aliasing rules. Please maintain the pattern and refrain from
      // using nested if statement.

      // Exactly same type and label.
      if (type1 == type2 && label1 == label2 && type1 != FieldDescriptorProto::TYPE_MESSAGE) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::BIDRECTIONAL));
        continue;
      }

      if (type1 == type2 && label1 == label2 && type1 == FieldDescriptorProto::TYPE_MESSAGE &&
          name1 == name2) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::BIDRECTIONAL));
        continue;
      }

      // Repeated numeric fields.
      if (label1 == label_repeated && label2 == label_repeated &&
          Contains(kNumericTypeSet, type1) && Contains(kNumericTypeSet, type2)) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::BIDRECTIONAL));
        continue;
      }

      // Repeated varint and string/bytes (repeated or optional) field.
      // Note that varint has a specific format. But if a string field violates the format, the
      // protobuf parsing will fail. That is outside of this function's scope. So it's
      // indistinguishable between repeated varint and string/bytes.
      if (label1 == label_repeated && Contains(kVarintTypeSet, type1) &&
          Contains(kStringAndBytes, type2)) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::BIDRECTIONAL));
        continue;
      }

      // string and bytes, optional and repeated, fields.
      if (Contains(kStringAndBytes, type1) && Contains(kStringAndBytes, type2)) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::BIDRECTIONAL));
        continue;
      }

      // message to string unidirectional.
      if (type1 == FieldDescriptorProto::TYPE_MESSAGE && Contains(kStringAndBytes, type2)) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::FORWARD));
        continue;
      }
      if (Contains(kStringAndBytes, type1) && type2 == FieldDescriptorProto::TYPE_MESSAGE) {
        res.push_back(CreateAliasingFieldPair(field1, field2, AliasingFieldPair::BACKWARD));
        continue;
      }
    }
  }
  return res;
}

void GetAliasingMethodPairs(const Service& service, std::vector<AliasingMethodPair>* res) {
  for (int i = 0; i < service.methods_size(); ++i) {
    for (int j = i + 1; j < service.methods_size(); ++j) {
      AliasingMethodPair pair;
      *pair.mutable_method1() = service.methods(i);
      *pair.mutable_method2() = service.methods(j);

      if (MessageDifferencer::Equivalent(pair.method1().req(), pair.method2().req()) &&
          MessageDifferencer::Equivalent(pair.method1().resp(), pair.method2().resp())) {
        // We do not consider equivalent messages.
        continue;
      }

      auto req_aliasing_field_pairs =
          GetAliasingFieldPairs(pair.method1().req(), pair.method2().req());
      auto resp_aliasing_field_pairs =
          GetAliasingFieldPairs(pair.method1().resp(), pair.method2().resp());
      for (auto& req_pair : req_aliasing_field_pairs) {
        *pair.add_reqs() = std::move(req_pair);
      }
      for (auto& resp_pair : resp_aliasing_field_pairs) {
        *pair.add_resps() = std::move(resp_pair);
      }
      ComputeAliasingProbability(&pair);
      res->push_back(std::move(pair));
    }
  }
}

double ComputeAliasingProbability(size_t msg1_field_count, size_t msg2_field_count,
                                  size_t aliasing_field_count) {
  // 1. Assume a serialized message is from one of req1 & req2.
  // 2. The probability of this message being aliasing between msg1 & msg2 is the
  // possibility that the serialized message only set fields that can potentially alias.
  // 3. That probability is 1/2^n, where n is the number of fields that are not the aliasing
  // fields, and assuming each field has 1/2 probability being set.

  // 1/p where p is the probability of msg1 only setting aliasing fields.
  double prob_msg1_aliasing = 0.5;
  const bool msg1_is_empty = msg1_field_count == 0;
  if (!msg1_is_empty) {
    prob_msg1_aliasing = std::pow(prob_msg1_aliasing, msg1_field_count - aliasing_field_count);
  }

  double prob_msg2_aliasing = 0.5;
  const bool msg2_is_empty = msg2_field_count == 0;
  if (!msg2_is_empty) {
    prob_msg2_aliasing = std::pow(prob_msg1_aliasing, msg2_field_count - aliasing_field_count);
  }

  if (msg1_is_empty && msg2_is_empty) {
    return 1.0;
  }
  if (msg1_is_empty) {
    return prob_msg2_aliasing;
  }
  if (msg2_is_empty) {
    return prob_msg1_aliasing;
  }
  return prob_msg1_aliasing * prob_msg2_aliasing;
}

void ComputeAliasingProbability(AliasingMethodPair* pair) {
  double p = ComputeAliasingProbability(pair->method1().req().field_size(),
                                        pair->method2().req().field_size(), pair->reqs_size()) *
             ComputeAliasingProbability(pair->method1().resp().field_size(),
                                        pair->method2().resp().field_size(), pair->resps_size());
  pair->set_p(p);
}

}  // namespace protobufs
}  // namespace experimental
