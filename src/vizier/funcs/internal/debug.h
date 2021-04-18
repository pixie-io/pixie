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

#ifdef TCMALLOC
#include <gperftools/malloc_extension.h>
#endif

namespace px {
namespace vizier {
namespace funcs {

constexpr int kMaxBufferSize = 1024 * 1024;

class HeapStatsUDTF final : public carnot::udf::UDTF<HeapStatsUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("heap", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pretty heap stats"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    std::string buf(kMaxBufferSize, '\0');
    MallocExtension::instance()->GetStats(&buf[0], buf.size());
    buf.resize(strlen(buf.c_str()));

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>(buf);
#else
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>("Only supported with tcmalloc.");
#endif

    return false;
  }
};

class HeapSampleUDTF final : public carnot::udf::UDTF<HeapSampleUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("heap", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pretty heap stats"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    std::string buf;
    MallocExtension::instance()->GetHeapSample(&buf);

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>(buf);
#else
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>("Only supported with tcmalloc.");
#endif

    return false;
  }
};

class HeapGrowthStacksUDTF final : public carnot::udf::UDTF<HeapGrowthStacksUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("heap", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pretty heap stats"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    std::string buf;
    MallocExtension::instance()->GetHeapGrowthStacks(&buf);

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>(buf);
#else
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>("Only supported with tcmalloc.");
#endif

    return false;
  }
};

}  // namespace funcs
}  // namespace vizier
}  // namespace px
