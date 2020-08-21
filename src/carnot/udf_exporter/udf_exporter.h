#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace udfexporter {

/**
 * @brief ExportUDFInfo() setups a RegistryInfo using all of the definitions that are
 * defined in the builtins. This greatly simplifies the need to carry around huge protobuf
 * strings in tests and other places, replacing it with a simple one line function call.
 *
 * @return StatusOr<std::unique_ptr<planner::RegistryInfo>>  a pointer to the resulting registry
 * info.
 */
StatusOr<std::unique_ptr<planner::RegistryInfo>> ExportUDFInfo();

/**
 * @brief ExportUDFDocs loads all the udfs into the registry and exports the docs.
 *
 * @return udfspb::Docs
 */
udfspb::Docs ExportUDFDocs();

}  // namespace udfexporter
}  // namespace carnot
}  // namespace pl
