#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace udfexporter {
/**
 * @brief GetUDFProto returns the proto for all of the udfs
 *
 * @return udfspb::UDFInfo: returns a proto that describes all of the functions in the repo.
 */
udfspb::UDFInfo GetUDFProto();

/**
 * @brief ExportUDFInfo() setups a RegistryInfo using all of the definitions that are
 * defined in the builtins. This greatly simplifies the need to carry around huge protobuf
 * strings in tests and other places, replacing it with a simple one line function call.
 *
 * @return StatusOr<std::unique_ptr<compiler::RegistryInfo>>  a pointer to the resulting registry
 * info.
 */
StatusOr<std::unique_ptr<compiler::RegistryInfo>> ExportUDFInfo();

/**
 * @brief Print out the udf registry in a nice format.
 *
 * @return std::string: the udf registry in a nice format.
 */
std::string UDFProtoFormatString();

}  // namespace udfexporter
}  // namespace carnot
}  // namespace pl
