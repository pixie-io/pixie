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

#include "experimental/protobufs/type_database.h"

#include <queue>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace experimental {
namespace protobufs {

using ::google::protobuf::DescriptorProto;
using ::google::protobuf::EnumDescriptorProto;
using ::google::protobuf::FieldDescriptorProto;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::ServiceDescriptorProto;

namespace {

void SearchNestedType(std::string_view package_name, const DescriptorProto& desc,
                      std::map<std::string, const DescriptorProto*>* type_map,
                      std::map<std::string, const EnumDescriptorProto*>* enum_type_map) {
  std::queue<const DescriptorProto*> desc_queue;
  desc_queue.push(&desc);
  std::vector<std::string> current_type_prefix = {std::string(package_name)};
  while (!desc_queue.empty()) {
    const auto* desc = desc_queue.front();
    desc_queue.pop();
    current_type_prefix.push_back(desc->name());
    for (const DescriptorProto& nest_desc : desc->nested_type()) {
      current_type_prefix.push_back(nest_desc.name());
      type_map->emplace(absl::StrJoin(current_type_prefix, "."), &nest_desc);
      current_type_prefix.pop_back();
      desc_queue.push(&nest_desc);
    }
    for (const EnumDescriptorProto& nest_enum_desc : desc->enum_type()) {
      current_type_prefix.push_back(nest_enum_desc.name());
      enum_type_map->emplace(absl::StrJoin(current_type_prefix, "."), &nest_enum_desc);
      current_type_prefix.pop_back();
    }
    current_type_prefix.pop_back();
  }
}

}  // namespace

void TypeDatabase::InitForProtoFiles(const std::vector<std::string>& files) {
  std::vector<FileDescriptorProto> file_descs;
  file_descs.reserve(files.size());

  std::map<std::string, const DescriptorProto*> type_map;
  std::map<std::string, const EnumDescriptorProto*> enum_type_map;
  std::map<std::string, const ServiceDescriptorProto*> service_type_map;
  for (const auto& file : files) {
    file_descs.emplace_back();
    desc_db_->FindFileByName(file, &file_descs.back());
    const FileDescriptorProto& file_desc = file_descs.back();

    for (const auto& msg_desc : file_desc.message_type()) {
      std::string msg_type_path = absl::StrCat(file_desc.package(), ".", msg_desc.name());
      type_map[std::move(msg_type_path)] = &msg_desc;
      SearchNestedType(file_desc.package(), msg_desc, &type_map, &enum_type_map);
    }
    for (const auto& enum_desc : file_desc.enum_type()) {
      std::string msg_type_path = absl::StrCat(file_desc.package(), ".", enum_desc.name());
      enum_type_map[std::move(msg_type_path)] = &enum_desc;
    }
    for (const auto& service_desc : file_desc.service()) {
      std::string service_path = absl::StrCat(file_desc.package(), ".", service_desc.name());
      service_type_map[std::move(service_path)] = &service_desc;
    }
  }

  auto get_search_paths = [](std::string_view service_path,
                             std::string_view type_path) -> std::vector<std::string> {
    if (absl::StartsWith(type_path, ".")) {
      type_path.remove_prefix(1);
      return {std::string(type_path)};
    } else {
      std::vector<std::string_view> service_path_components =
          absl::StrSplit(service_path, ".", absl::SkipEmpty());
      std::vector<std::string> res;
      for (size_t i = 0, loop_count = service_path_components.size() + 1; i < loop_count; ++i) {
        service_path_components.push_back(type_path);
        res.push_back(absl::StrJoin(service_path_components, "."));
        service_path_components.pop_back();
        if (!service_path_components.empty()) {
          service_path_components.pop_back();
        }
      }
      return res;
    }
  };
  for (const auto& [service_path, service_desc] : service_type_map) {
    Service service;
    service.set_name(service_path);
    for (const auto& method : service_desc->method()) {
      Service::Method method_desc;
      method_desc.set_name(absl::StrCat(service_path, ".", method.name()));
      for (const auto& search_path : get_search_paths(service_path, method.input_type())) {
        auto iter = type_map.find(search_path);
        if (iter != type_map.end()) {
          *method_desc.mutable_req() = *iter->second;
          method_desc.mutable_req()->set_name(search_path);
          break;
        }
      }
      for (const auto& search_path : get_search_paths(service_path, method.output_type())) {
        auto iter = type_map.find(search_path);
        if (iter != type_map.end()) {
          *method_desc.mutable_resp() = *iter->second;
          method_desc.mutable_resp()->set_name(search_path);
          break;
        }
      }
      for (auto& field_desc : *method_desc.mutable_req()->mutable_field()) {
        if (field_desc.has_type()) {
          continue;
        }
        for (const auto& search_path :
             get_search_paths(method_desc.req().name(), field_desc.type_name())) {
          if (type_map.find(search_path) != type_map.end()) {
            field_desc.set_type(FieldDescriptorProto::TYPE_MESSAGE);
            break;
          }
          if (enum_type_map.find(search_path) != enum_type_map.end()) {
            field_desc.set_type(FieldDescriptorProto::TYPE_ENUM);
            break;
          }
        }
      }
      for (auto& field_desc : *method_desc.mutable_resp()->mutable_field()) {
        if (field_desc.has_type()) {
          continue;
        }
        for (const auto& search_path :
             get_search_paths(method_desc.resp().name(), field_desc.type_name())) {
          if (type_map.find(search_path) != type_map.end()) {
            field_desc.set_type(FieldDescriptorProto::TYPE_MESSAGE);
            break;
          }
          if (enum_type_map.find(search_path) != enum_type_map.end()) {
            field_desc.set_type(FieldDescriptorProto::TYPE_ENUM);
            break;
          }
        }
      }
      *service.add_methods() = std::move(method_desc);
    }
    service_descs_.push_back(std::move(service));
    service_desc_map_[service_path] = &service_descs_.back();
  }
}

}  // namespace protobufs
}  // namespace experimental
