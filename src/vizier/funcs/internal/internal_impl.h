#pragma once

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/shared/version/version.h"

namespace px {
namespace vizier {
namespace funcs {
namespace internal {

class KelvinVersionUDTF final : public carnot::udf::UDTF<KelvinVersionUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("version", types::DataType::STRING, types::PatternType::GENERAL,
                             "The version of kelvin"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("version")>(VersionInfo::VersionString());

    return false;
  }
};

void RegisterFuncsOrDie(carnot::udf::Registry* registry);

}  // namespace internal
}  // namespace funcs
}  // namespace vizier
}  // namespace px
