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
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

struct ConnectorStatusRecord {
  uint64_t timestamp_ns = 0;
  std::string source_connector = "";
  uint64_t status = 0;
  std::string error = "";
};

/**
 * The ConnectorStatusStore is used to store the initialization status and error messages from
 * different source connectors. Data in the store is pulled periodically by the
 * StirlingErrorConnector and appended in the stirling error table, so that it can provide statuses
 * and error messages of the connectors to users.
 */
class ConnectorStatusStore {
 public:
  void AppendRecord(const std::string& source_connector, const Status& init_status) {
    connector_status_records_.push_back({static_cast<uint64_t>(CurrentTimeNS()), source_connector,
                                         static_cast<uint64_t>(init_status.code()),
                                         init_status.msg()});
  }

  std::vector<ConnectorStatusRecord> ConsumeRecords() {
    return std::move(connector_status_records_);
  }

  void Clear() { connector_status_records_.clear(); }

 private:
  std::vector<ConnectorStatusRecord> connector_status_records_;
};

}  // namespace stirling
}  // namespace px
