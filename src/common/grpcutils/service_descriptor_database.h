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
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace grpc {

struct MethodInputOutput {
  std::unique_ptr<google::protobuf::Message> input;
  std::unique_ptr<google::protobuf::Message> output;
};

/**
 * @brief Indexes services and the descriptors of their methods' input and output protobuf messages.
 */
class ServiceDescriptorDatabase {
 public:
  explicit ServiceDescriptorDatabase(google::protobuf::FileDescriptorSet fdset);

  /**
   * @brief Returns empty instances of the input and output type of the method specified by the
   * input method path.
   *
   * @param method_path A dot-separated name including the service name.
   */
  MethodInputOutput GetMethodInputOutput(const std::string& method_path);

  /**
   * @brief Returns an empty instance of the message specified by the input path.
   *
   * @param message_path A dot-separated name.
   */
  std::unique_ptr<google::protobuf::Message> GetMessage(const std::string& message_path);

  /**
   * @brief Returns all services in the descriptor database.
   *
   * Note that this function was added for message type inference, when the message type
   * of a message is not known. This function should not be necessary in code that
   * knows the message type of a message.
   *
   * @return vector of service descriptor protos.
   */
  std::vector<google::protobuf::ServiceDescriptorProto> AllServices();

 private:
  google::protobuf::SimpleDescriptorDatabase desc_db_;
  google::protobuf::DescriptorPool desc_pool_;
  google::protobuf::DynamicMessageFactory message_factory_;
};

// TODO(yzhao): Benchmark dynamic message parsing.

}  // namespace grpc
}  // namespace px
