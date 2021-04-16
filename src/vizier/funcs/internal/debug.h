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
