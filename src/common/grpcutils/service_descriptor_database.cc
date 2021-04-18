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

#include "src/common/grpcutils/service_descriptor_database.h"

#include <string>

#include <absl/memory/memory.h>
#include "src/common/base/logging.h"

namespace px {
namespace grpc {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::ServiceDescriptor;
using ::google::protobuf::SimpleDescriptorDatabase;

ServiceDescriptorDatabase::ServiceDescriptorDatabase(FileDescriptorSet fdset)
    : desc_db_(), desc_pool_(&desc_db_), message_factory_() {
  for (const auto& fd : fdset.file()) {
    if (!desc_db_.Add(fd)) {
      LOG(ERROR) << "Failed to insert FileDescriptorProto for file: " << fd.name();
    }
  }
}

MethodInputOutput ServiceDescriptorDatabase::GetMethodInputOutput(const std::string& method_path) {
  const MethodDescriptor* method = desc_pool_.FindMethodByName(method_path);
  if (method == nullptr) {
    return {nullptr, nullptr};
  }

  MethodInputOutput res;

  const Message* input_prototype = message_factory_.GetPrototype(method->input_type());
  if (input_prototype != nullptr) {
    res.input.reset(input_prototype->New());
  }
  const Message* output_prototype = message_factory_.GetPrototype(method->output_type());
  if (output_prototype != nullptr) {
    res.output.reset(output_prototype->New());
  }
  return res;
}

std::unique_ptr<Message> ServiceDescriptorDatabase::GetMessage(const std::string& message_path) {
  const Descriptor* msg_desc = desc_pool_.FindMessageTypeByName(message_path);
  if (msg_desc == nullptr) {
    return nullptr;
  }
  const Message* msg_prototype = message_factory_.GetPrototype(msg_desc);
  if (msg_prototype == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<Message>(msg_prototype->New());
}

std::vector<::google::protobuf::ServiceDescriptorProto> ServiceDescriptorDatabase::AllServices() {
  std::vector<google::protobuf::ServiceDescriptorProto> services;

  std::vector<std::string> filenames;
  desc_db_.FindAllFileNames(&filenames);
  for (auto& filename : filenames) {
    FileDescriptorProto desc_proto;
    desc_db_.FindFileByName(filename, &desc_proto);
    for (const auto& service : desc_proto.service()) {
      services.push_back(service);
    }
  }
  return services;
}

}  // namespace grpc
}  // namespace px
