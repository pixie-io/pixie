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
 * @brief This class maintains a registry of all the sources available to the
 * the data collector.
 * The registry contains a map of source name -> RegistryElements.
 * RegistryElement contains the source type and an std::function using which
 * the data collector can construct a SourceConnector.
 */
class SourceRegistry : public NotCopyable {
 public:
  SourceRegistry() = default;
  virtual ~SourceRegistry() = default;

  struct RegistryElement {
    RegistryElement() : create_source_fn(nullptr), schema(ArrayView<DataTableSchema>()) {}
    explicit RegistryElement(
        std::function<std::unique_ptr<SourceConnector>(std::string_view)> create_source_fn,
        const ArrayView<DataTableSchema>& schema)
        : create_source_fn(std::move(create_source_fn)), schema(schema) {}
    std::function<std::unique_ptr<SourceConnector>(std::string_view)> create_source_fn;
    ArrayView<DataTableSchema> schema;
  };

  const auto& sources() { return sources_map_; }

  /**
   * @brief Register a source in the registry.
   *
   * @tparam TSourceConnector SourceConnector Class for a data source
   * @param name  Name of the data source
   * @return Status
   */
  template <typename TSourceConnector>
  Status Register(const std::string& name) {
    // Create registry element from template
    SourceRegistry::RegistryElement element(TSourceConnector::Create, TSourceConnector::kTables);
    if (sources_map_.find(name) != sources_map_.end()) {
      return error::AlreadyExists("The data source with name \"$0\" already exists", name);
    }
    sources_map_.insert({name, element});

    return Status::OK();
  }

  template <typename TSourceConnector>
  void RegisterOrDie(const std::string& name) {
    auto status = Register<TSourceConnector>(name);
    PL_CHECK_OK(status);
  }

 protected:
  std::unordered_map<std::string, RegistryElement> sources_map_;
};

}  // namespace stirling
}  // namespace px
