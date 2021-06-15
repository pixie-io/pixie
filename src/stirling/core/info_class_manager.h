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
#include "src/stirling/core/sample_push_frequency_manager.h"
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
  explicit InfoClassManager(const DataTableSchema& schema,
                            stirlingpb::SourceType type = stirlingpb::STATIC)
      : type_(type), schema_(schema), data_table_(new DataTable(schema_)) {
    id_ = global_id_++;
    sample_push_freq_mgr_.set_push_period(schema.default_push_period());
  }

  /**
   * Resets the data table, effectively replace the current data table with a new one.
   * Used to reset the state of subscribed data tables.
   */
  void ResetDataTable() { data_table_.reset(new DataTable(schema_)); }

  /**
   * @brief Source connector connected to this Info Class.
   *
   * @param source Pointer to source connector instance.
   */
  void SetSourceConnector(SourceConnector* source, uint32_t table_num) {
    source_ = source;
    source_table_num_ = table_num;
  }

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
   * Configure sampling period.
   *
   * @param period Sampling period in ms.
   */
  void SetPushPeriod(std::chrono::milliseconds period) {
    sample_push_freq_mgr_.set_push_period(period);
  }

  /**
   * Returns true if a data push is required, for whatever reason (elapsed time, occupancy, etc.).
   *
   * @return bool
   */
  bool PushRequired() const;

  /**
   * Set initial context. Meant to be run once as an initialization step.
   */
  void InitContext(ConnectorContext* ctx);

  /**
   * Push data by using the callback.
   */
  void PushData(DataPushCallback agent_callback);

  /**
   * Notify function to update state after making changes to the schema.
   * This will make sure changes are pushed to the Source Connector and Data Tables accordingly.
   */
  void Notify() {}

  /**
   * Returns the next time the data table needs to be pushed upstream, according to the push period.
   *
   * @return std::chrono::milliseconds
   */
  px::chrono::coarse_steady_clock::time_point NextPushTime() const {
    return sample_push_freq_mgr_.NextPushTime();
  }

  /**
   * Set the Subscription for the InfoClass.
   *
   * @param subscription
   */
  void SetSubscription(bool subscribed) { subscribed_ = subscribed; }

  std::string_view name() const { return schema_.name(); }
  const SourceConnector* source() const { return source_; }
  uint64_t id() const { return id_; }
  bool subscribed() const { return subscribed_; }
  uint32_t source_table_num() const { return source_table_num_; }
  DataTable* data_table() const { return data_table_.get(); }

 private:
  inline static std::atomic<uint64_t> global_id_ = 0;

  stirlingpb::SourceType type_;

  // Unique ID of the InfoClassManager instance. ID must never repeat, even after destruction.
  uint64_t id_;

  // The schema of table associated with this Info Class manager.
  const DataTableSchema& schema_;

  // Boolean indicating whether an agent has subscribed to the Info Class.
  bool subscribed_ = false;

  // Pointer back to the source connector providing the data.
  SourceConnector* source_ = nullptr;

  // Table number within source connector for this info class.
  uint32_t source_table_num_;

  // Pointer to the data table where the data is stored.
  std::unique_ptr<DataTable> data_table_;

  // Used to determine push frequency.
  // NOTE: The sampling period part is not used!
  SamplePushFrequencyManager sample_push_freq_mgr_;
};

using InfoClassManagerVec = std::vector<std::unique_ptr<InfoClassManager>>;

}  // namespace stirling
}  // namespace px
