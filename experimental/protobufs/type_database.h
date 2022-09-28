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

#include "experimental/protobufs/proto/alias.pb.h"
#include "src/common/grpcutils/service_descriptor_database.h"

namespace experimental {
namespace protobufs {

/**
 * @brief A database of protobuf descriptor protobufs. Unlike DescriptorDatabase, it does not try to
 * create dynamic messages, only returns shallow descriptor protobufs.
 */
class TypeDatabase {
 public:
  TypeDatabase(::google::protobuf::DescriptorDatabase* desc_db)
      : desc_db_(desc_db), service_descs_(), service_desc_map_() {}

  void InitForProtoFiles(const std::vector<std::string>& files);

  auto service_descs() const { return service_descs_; }

 private:
  ::google::protobuf::DescriptorDatabase* desc_db_ = nullptr;
  std::vector<Service> service_descs_;
  // Map from service name to its method types.
  std::map<std::string, const Service*> service_desc_map_;
};

}  // namespace protobufs
}  // namespace experimental
