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

#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/frequency_manager.h"

/**
 * These are the steps to follow to add a new data source connector.
 * 1. If required, create a new SourceConnector class.
 * 2. Add a new Create function with the following signature:
 *    static std::unique_ptr<SourceConnector> Create().
 *    In this function create an InfoClassSchema (vector of DataElement)
 * 3. Register the data source in the appropriate registry.
 */

namespace px {
namespace stirling {

class SourceConnector : public NotCopyable {
 public:
  SourceConnector() = delete;
  virtual ~SourceConnector() = default;

  /**
   * Initializes the source connector. Can only be called once.
   * @return Status of whether initialization was successful.
   */
  Status Init();

  /**
   * Sets the initial context for the source connector.
   * Used for context specific init steps (e.g. deploying uprobes on PIDs).
   */
  void InitContext(ConnectorContext* ctx);

  /**
   * Transfers all collected data to data tables.
   * @param ctx Shared context, e.g. ASID & tracked PIDs.
   * @param data_tables Map from the table number to DataTable objects.
   */
  void TransferData(ConnectorContext* ctx, const std::vector<DataTable*>& data_tables);

  /**
   * Pushes data in data tables into table store.
   */
  void PushData(DataPushCallback agent_callback, const std::vector<DataTable*>& data_tables);

  /**
   * Stops the source connector and releases any acquired resources.
   * May only be called after a successful Init().
   *
   * @return Status of whether stop was successful.
   */
  Status Stop();

  const std::string& name() const { return source_name_; }

  const ArrayView<DataTableSchema>& table_schemas() const { return table_schemas_; }

  static constexpr uint32_t TableNum(ArrayView<DataTableSchema> tables,
                                     const DataTableSchema& key) {
    uint32_t i = 0;
    for (i = 0; i < tables.size(); i++) {
      if (tables[i].name() == key.name()) {
        break;
      }
    }

    // Check that we found the index. This prevents compilation if name is not found,
    // during constexpr evaluation (which is awesome!).
    COMPILE_TIME_ASSERT(i != tables.size(), "Could not find name");

    return i;
  }

  /**
   * Utility function to convert time as recorded by in monotonic clock to real time.
   * This is especially useful for converting times from BPF, which are all in monotonic clock.
   */
  uint64_t ClockRealTimeOffset() const { return sysconfig_.ClockRealTimeOffset(); }

  virtual void SetDebugLevel(int level) { debug_level_ = level; }
  virtual void EnablePIDTrace(int pid) { pids_to_trace_.insert(pid); }
  virtual void DisablePIDTrace(int pid) { pids_to_trace_.erase(pid); }

  const FrequencyManager& sampling_freq_mgr() const { return sampling_freq_mgr_; }
  const FrequencyManager& push_freq_mgr() const { return push_freq_mgr_; }

 protected:
  explicit SourceConnector(std::string_view source_name,
                           const ArrayView<DataTableSchema>& table_schemas)
      : source_name_(source_name), table_schemas_(table_schemas) {}

  virtual Status InitImpl() = 0;

  // Provide a default InitContextImpl which does nothing.
  // SourceConnectors only need override if action is required on the initial context.
  virtual void InitContextImpl(ConnectorContext* /* ctx */) {}

  virtual void TransferDataImpl(ConnectorContext*, const std::vector<DataTable*>&) = 0;

  virtual Status StopImpl() = 0;

 protected:
  /**
   * Track state of connector. A connector's lifetime typically progresses sequentially
   * from kUninitialized -> kActive -> KStopped.
   *
   * kErrors is a special state to track a bad state.
   */
  enum class State { kUninitialized, kActive, kStopped, kErrors };

  // Sub-classes are allowed to inspect state.
  State state() const { return state_; }

  const system::Config& sysconfig_ = system::Config::GetInstance();

  FrequencyManager sampling_freq_mgr_;
  FrequencyManager push_freq_mgr_;

  // Debug members.
  int debug_level_ = 0;
  absl::flat_hash_set<int> pids_to_trace_;

 private:
  std::atomic<State> state_ = State::kUninitialized;

  const std::string source_name_;
  const ArrayView<DataTableSchema> table_schemas_;
};

}  // namespace stirling
}  // namespace px
