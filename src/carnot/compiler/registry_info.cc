#include <glog/logging.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/proto/udfs.pb.h"
#include "src/common/types/types.pb.h"

#include "src/common/error.h"
#include "src/common/statusor.h"

namespace pl {
namespace carnot {
namespace compiler {

Status RegistryInfo::Init(const carnotpb::UDFInfo info) {
  for (auto uda : info.udas()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(uda.update_arg_types_size());

    for (int64_t i = 0; i < uda.update_arg_types_size(); i++) {
      arg_types.push_back(uda.update_arg_types(i));
    }
    auto key = RegistryKey(uda.name(), arg_types);
    uda_map_[key] = uda.finalize_type();
  }

  for (auto udf : info.scalar_udfs()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(udf.exec_arg_types_size());

    for (int64_t i = 0; i < udf.exec_arg_types_size(); i++) {
      arg_types.push_back(udf.exec_arg_types(i));
    }
    auto key = RegistryKey(udf.name(), arg_types);
    udf_map_[key] = udf.return_type();
  }

  return Status::OK();
}

StatusOr<types::DataType> RegistryInfo::GetUDA(std::string name,
                                               std::vector<types::DataType> update_arg_types) {
  auto uda = uda_map_.find(RegistryKey(name, update_arg_types));
  if (uda == uda_map_.end()) {
    return error::InvalidArgument("Could not find UDA");
  }
  return uda->second;
}

StatusOr<types::DataType> RegistryInfo::GetUDF(std::string name,
                                               std::vector<types::DataType> exec_arg_types) {
  auto udf = udf_map_.find(RegistryKey(name, exec_arg_types));
  if (udf == udf_map_.end()) {
    return error::InvalidArgument("Could not find UDF");
  }
  return udf->second;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
