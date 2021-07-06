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

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/frequency_manager.h"
#include "src/stirling/core/types.h"
#include "src/stirling/proto/stirling.pb.h"

namespace px {
namespace stirling {

class ConnectorContext;
class SourceConnector;
class DataTable;

/**
 * InfoClassManager is the unit responsible for managing a data source, and its data transfers.
 *
 * InfoClassManager samples the data from the source, transferring the data to an internal table.
 * It also initiates data transfers to the TableStore.
 *
 * The InfoClassManager also serves as the State Manager for the entire data collector.
 *  - The Config unit uses the Schemas to publish available data to the Agent.
 *  - The Config unit changes the state of elements based on the Publish call from the Agent.
 *  - There is a 1:1 relationship with the Data Tables.
 *  - Each InfoClassManager points back to its SourceConnector.
 */
class InfoClassManager final : public NotCopyable {
 public:
  /**
   * @brief Construct a new Info Class Manager object
   * SourceConnector constructs InfoClassManager objects with and adds Elements to it
   *
   * @param name Name of the InfoClass
   * @param source Pointer to the SourceConnector that created the InfoClassManager object.
   * This is required to identify an InfoClassManager parent source and also to generate
   * the publish proto.
   */
  explicit InfoClassManager(const DataTableSchema& schema)
      : id_(global_id_++), schema_(schema), data_table_(new DataTable(id_, schema_)) {}

  /**
   * @brief Source connector connected to this Info Class.
   *
   * @param source Pointer to source connector instance.
   */
  void SetSourceConnector(SourceConnector* source) { source_ = source; }

  /**
   * Get the schema of the InfoClass.
   *
   * @return DataTableSchema schema
   */
  const DataTableSchema& Schema() const { return schema_; }

  /**
   * Get an Element object
   *
   * @param index
   * @return DataElement
   */
  const DataElement& GetElement(size_t index) const {
    DCHECK(index < schema_.elements().size());
    return schema_.elements()[index];
  }

  /**
   * Generate a proto message based on the InfoClassManager.
   *
   * @return stirlingpb::InfoClass
   */
  stirlingpb::InfoClass ToProto() const;

  /**
   * Set initial context. Meant to be run once as an initialization step.
   */
  void InitContext(ConnectorContext* ctx);

  /**
   * Notify function to update state after making changes to the schema.
   * This will make sure changes are pushed to the Source Connector and Data Tables accordingly.
   */
  void Notify() {}

  std::string_view name() const { return schema_.name(); }
  const SourceConnector* source() const { return source_; }
  uint64_t id() const { return id_; }
  DataTable* data_table() const { return data_table_.get(); }

 private:
  inline static std::atomic<uint64_t> global_id_ = 0;

  // Unique ID of the InfoClassManager instance. ID must never repeat, even after destruction.
  const uint64_t id_;

  // The schema of table associated with this Info Class manager.
  const DataTableSchema& schema_;

  // Pointer back to the source connector providing the data.
  SourceConnector* source_ = nullptr;

  // Pointer to the data table where the data is stored.
  std::unique_ptr<DataTable> data_table_;
};

using InfoClassManagerVec = std::vector<std::unique_ptr<InfoClassManager>>;

}  // namespace stirling
}  // namespace px
