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

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "threadstacks/signal_handler.h"

namespace px {
namespace vizier {
namespace funcs {

class StackTracerUDTF final : public carnot::udf::UDTF<StackTracerUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("stack_trace", types::DataType::STRING, types::PatternType::GENERAL,
                             "The stack of all the current executing threads"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    std::string error;
    threadstacks::StackTraceCollector collector;
    auto results = collector.Collect(&error);
    if (results.empty()) {
      LOG(ERROR) << "StackTrace collection failed: " << error << std::endl;
      return false;
    }
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("stack_trace")>(threadstacks::StackTraceCollector::ToPrettyString(results));

    return false;
  }
};

}  // namespace funcs
}  // namespace vizier
}  // namespace px
