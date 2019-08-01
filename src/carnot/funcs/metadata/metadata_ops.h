#pragma once

#include <string>

#include "src/carnot/udf/registry.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace funcs {
namespace metadata {

using ScalarUDF = pl::carnot::udf::ScalarUDF;
using FunctionContext = pl::carnot::udf::FunctionContext;

inline const pl::md::AgentMetadataState* GetMetadataState(FunctionContext* ctx) {
  DCHECK(ctx != nullptr);
  auto md = ctx->metadata_state();
  DCHECK(md != nullptr);
  return md;
}

class ASIDUDF : public ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext* ctx) {
    auto md = GetMetadataState(ctx);
    return md->asid();
  }
};

class PodIDToPodNameUDF : public ScalarUDF {
 public:
  types::StringValue Exec(FunctionContext* ctx, types::StringValue pod_id) {
    auto md = GetMetadataState(ctx);

    const auto* pod_info = md->k8s_metadata_state().PodInfoByID(pod_id);
    if (pod_info != nullptr) {
      return absl::Substitute("$0/$1", pod_info->ns(), pod_info->name());
    }

    return "";
  }
};

void RegisterMetadataOpsOrDie(pl::carnot::udf::ScalarUDFRegistry* registry);

}  // namespace metadata
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
