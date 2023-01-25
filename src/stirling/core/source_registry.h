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

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

/**
 * This class maintains a registry of all the sources available to the
 * the data collector.
 * The registry contains a map of source name -> RegistryElements.
 * RegistryElement contains the source type and an std::function using which
 * the data collector can construct a SourceConnector.
 */
class SourceRegistry : public NotCopyable {
 public:
  struct RegistryElement {
    std::string name;
    std::function<std::unique_ptr<SourceConnector>(std::string_view)> create_source_fn;
    ArrayView<DataTableSchema> schema;
  };

  template <typename TSourceConnector>
  static RegistryElement CreateRegistryElement(std::string_view name) {
    return {std::string(name), TSourceConnector::Create, TSourceConnector::kTables};
  }

  Status Register(RegistryElement registry_element) {
    if (registry_element.name.empty()) {
      return error::InvalidArgument("Name cannot be empty.");
    }

    for (const auto& entry : sources_) {
      if (entry.name == registry_element.name) {
        return error::AlreadyExists("The data source with name \"$0\" already exists",
                                    registry_element.name);
      }
    }

    sources_.push_back(std::move(registry_element));
    return Status::OK();
  }

  template <typename TSourceConnector>
  void RegisterOrDie(std::string_view name = {}) {
    if (name.empty()) {
      name = TSourceConnector::kName;
    }
    auto status = Register(CreateRegistryElement<TSourceConnector>(name));
    PX_CHECK_OK(status);
  }

  const auto& sources() { return sources_; }

 private:
  std::vector<RegistryElement> sources_;
};

}  // namespace stirling
}  // namespace px
