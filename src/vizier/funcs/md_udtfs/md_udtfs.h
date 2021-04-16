#pragma once

#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/vizier/funcs/context/vizier_context.h"

namespace px {
namespace vizier {
namespace funcs {
namespace md {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry);

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace px
