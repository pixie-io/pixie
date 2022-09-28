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

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>

#include "experimental/protobufs/proto/alias.pb.h"

namespace experimental {
namespace protobufs {

/**
 * @brief Determines if lhs is more specialized than rhs.
 *
 * @return -1 if lhs is more specialized than rhs; 0 if could not determine; 1 if lhs is less
 * specialized than rhs.
 *
 * @note This is meant to be used as part of gRPC's trial parsing in socket tracer, when the tracer
 * cannot decode the headers. It works as follows:
 * - From metadata service, get a list of req & resp protobuf pairs for the possible RPC methods of
 *   an endpoint.
 * - Use the protobufs to parse the serialized protobuf messages.
 * - Discard the protobufs that 1) encounter parse failures; 2) has any unknown fields.
 * - For the remaining protobufs, sort them using **THIS ORDERING FUNCTION**.
 */
// TODO(yzhao): This function now only prefer message field over string field.
// Extend to return more complicated cases.
int IsMoreSpecialized(const ::google::protobuf::Message& lhs,
                      const ::google::protobuf::Message& rhs);

/**
 * @brief Returns a list of descriptor proto to the fields that can potentially alias, including
 * the fields that are same.
 */
std::vector<AliasingFieldPair> GetAliasingFieldPairs(
    const ::google::protobuf::DescriptorProto& first,
    const ::google::protobuf::DescriptorProto& second);

/**
 * @brief Returns a list of pairs of aliasing field for all methods of a service.
 */
void GetAliasingMethodPairs(const Service& service, std::vector<AliasingMethodPair>* res);

/**
 * @brief Given a pair of method, computes the probability of a pair of serialized protobuf messages
 * being aliasing between 2 request and response protobufs, respectively.
 */
void ComputeAliasingProbability(AliasingMethodPair* pair);

}  // namespace protobufs
}  // namespace experimental
