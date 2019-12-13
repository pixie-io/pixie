#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include "src/carnot/compiler/compiler_state/registry_info.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/proto/types.pb.h"

namespace pl {
namespace carnot {
namespace compiler {

Status RegistryInfo::Init(const udfspb::UDFInfo info) {
  for (auto uda : info.udas()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(uda.update_arg_types_size());

    for (int64_t i = 0; i < uda.update_arg_types_size(); i++) {
      arg_types.push_back(uda.update_arg_types(i));
    }
    auto key = RegistryKey(uda.name(), arg_types);
    uda_map_[key] = uda.finalize_type();
    // Add uda to func_names_.
    func_names_.insert(uda.name());
  }

  for (auto udf : info.scalar_udfs()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(udf.exec_arg_types_size());

    for (int64_t i = 0; i < udf.exec_arg_types_size(); i++) {
      arg_types.push_back(udf.exec_arg_types(i));
    }

    auto key = RegistryKey(udf.name(), arg_types);
    udf_map_[key] = udf.return_type();
    // Add udf to func_names_.
    func_names_.insert(udf.name());
  }

  return Status::OK();
}

StatusOr<types::DataType> RegistryInfo::GetUDA(std::string name,
                                               std::vector<types::DataType> update_arg_types) {
  auto uda = uda_map_.find(RegistryKey(name, update_arg_types));
  if (uda == uda_map_.end()) {
    return error::InvalidArgument("Could not find UDA '$0' with update arg types [$1].", name,
                                  absl::StrJoin(update_arg_types, ","));
  }
  return uda->second;
}

StatusOr<types::DataType> RegistryInfo::GetUDF(std::string name,
                                               std::vector<types::DataType> exec_arg_types) {
  auto udf = udf_map_.find(RegistryKey(name, exec_arg_types));
  if (udf == udf_map_.end()) {
    std::vector<std::string> arg_data_type_strs;
    for (const types::DataType& arg_data_type : exec_arg_types) {
      arg_data_type_strs.push_back(types::DataType_Name(arg_data_type));
    }
    return error::InvalidArgument("Could not find function '$0' with arguments [$1].", name,
                                  absl::StrJoin(arg_data_type_strs, ","));
  }
  return udf->second;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
