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

#include <google/protobuf/compiler/importer.h>

#include <filesystem>
#include <queue>

#include "absl/strings/str_split.h"
#include "experimental/protobufs/aliasing.h"
#include "experimental/protobufs/proto/alias.pb.h"
#include "experimental/protobufs/type_database.h"
#include "src/common/base/base.h"
#include "src/common/grpcutils/service_descriptor_database.h"

DEFINE_string(base_path, ".", "The path to the base directory to look up .proto files.");
DEFINE_string(proto_files, "", "Comma-separated relative paths to .proto files under base_path.");

using ::experimental::AliasingMethodPair;
using ::experimental::Service;
using ::experimental::protobufs::ComputeAliasingProbability;
using ::experimental::protobufs::GetAliasingMethodPairs;
using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorDatabase;
using ::google::protobuf::DescriptorProto;
using ::google::protobuf::EnumDescriptorProto;
using ::google::protobuf::FieldDescriptorProto;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::ServiceDescriptorProto;
using ::google::protobuf::SimpleDescriptorDatabase;
using ::google::protobuf::compiler::DiskSourceTree;
using ::google::protobuf::compiler::SourceTreeDescriptorDatabase;
using ::px::grpc::ServiceDescriptorDatabase;

namespace experimental {

std::vector<std::string> ListAllProtoFiles(std::string_view base_path) {
  std::vector<std::string> res;
  for (const auto& path : std::filesystem::recursive_directory_iterator(base_path)) {
    if (absl::EndsWith(path.path().string(), ".proto")) {
      res.push_back(path.path().string().substr(base_path.size() + 1));
    }
  }
  return res;
}

}  // namespace experimental

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  const std::filesystem::path base(FLAGS_base_path);
  std::vector<std::string> files = absl::StrSplit(FLAGS_proto_files, ",", absl::SkipEmpty());

  if (files.empty()) {
    files = experimental::ListAllProtoFiles(FLAGS_base_path);
  } else {
    // Remove non-existent files.
    files.erase(
        std::remove_if(files.begin(), files.end(),
                       [&base](const std::string& path) {
                         return !std::filesystem::exists(base / std::filesystem::path(path));
                       }),
        files.end());
  }

  // TODO(yzhao/oazizi): Another option is to use protc tool to generate FileDescriptorSet
  // from a list of .proto files. The downside is that require subprocessing a binary call.
  DiskSourceTree source_tree;

  for (auto file : files) {
    std::filesystem::path p(file);
    const std::filesystem::path disk_path = base / p;
    source_tree.MapPath(file, disk_path);
  }

  auto db = std::make_unique<SourceTreeDescriptorDatabase>(&source_tree);
  auto* db_copy = db.get();

  experimental::protobufs::TypeDatabase type_db(db_copy);
  type_db.InitForProtoFiles(files);
  std::vector<AliasingMethodPair> aliasing_method_pairs;
  for (const auto& service : type_db.service_descs()) {
    LOG(INFO) << "service: " << service.name() << ": " << service.methods_size();
    for (const auto& method : service.methods()) {
      LOG(INFO) << "method: " << method.name() << " req_fields: " << method.req().field_size()
                << "resp_fields: " << method.resp().field_size();
    }
    GetAliasingMethodPairs(service, &aliasing_method_pairs);
  }
  for (const auto& service : type_db.service_descs()) {
    LOG(INFO) << "service: " << service.DebugString();
  }
  std::sort(aliasing_method_pairs.begin(), aliasing_method_pairs.end(),
            [](const AliasingMethodPair& lhs, const AliasingMethodPair& rhs) {
              return lhs.p() < rhs.p();
            });
  for (const auto& pair : aliasing_method_pairs) {
    LOG(INFO) << pair.method1().name() << ":" << pair.method2().name() << " " << pair.p();
  }
  for (const auto& pair : aliasing_method_pairs) {
    LOG(INFO) << pair.DebugString();
  }
}
