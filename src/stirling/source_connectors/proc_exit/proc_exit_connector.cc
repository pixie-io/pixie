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

#include "src/stirling/source_connectors/proc_exit/proc_exit_connector.h"

#include "src/common/base/base.h"

namespace px {
namespace stirling {

Status ProcExitConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  return Status::OK();
}

void ProcExitConnector::TransferDataImpl(ConnectorContext* /*ctx*/,
                                         const std::vector<DataTable*>& /*data_tables*/) {
  LOG(DFATAL) << "Not implemented yet";
}

}  // namespace stirling
}  // namespace px
