#pragma once

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/vizier/funcs/context/vizier_context.h"

namespace px {
namespace vizier {
namespace funcs {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry);

}  // namespace funcs
}  // namespace vizier
}  // namespace px
