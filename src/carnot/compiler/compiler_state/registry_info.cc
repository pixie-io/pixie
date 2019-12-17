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

Status RegistryInfo::Init(const udfspb::UDFInfo& info) {
  for (const auto& uda : info.udas()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(uda.update_arg_types_size());

    for (int64_t i = 0; i < uda.update_arg_types_size(); i++) {
      arg_types.push_back(uda.update_arg_types(i));
    }
    auto key = RegistryKey(uda.name(), arg_types);
    uda_map_[key] = uda.finalize_type();
    // Add uda to funcs_.
    if (funcs_.contains(uda.name())) {
      PL_ASSIGN_OR_RETURN(auto type, GetUDFType(uda.name()));
      DCHECK(UDFType::kUDA == type);
    }
    funcs_[uda.name()] = UDFType::kUDA;
  }

  for (const auto& udf : info.scalar_udfs()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(udf.exec_arg_types_size());

    for (int64_t i = 0; i < udf.exec_arg_types_size(); i++) {
      arg_types.push_back(udf.exec_arg_types(i));
    }

    auto key = RegistryKey(udf.name(), arg_types);
    udf_map_[key] = udf.return_type();
    // Add udf to funcs_.
    if (funcs_.contains(udf.name())) {
      PL_ASSIGN_OR_RETURN(auto type, GetUDFType(udf.name()));
      DCHECK(UDFType::kUDF == type);
    }
    funcs_[udf.name()] = UDFType::kUDF;
  }

  for (const auto& udtf : info.udtfs()) {
    AddUDTF(udtf);
  }

  return Status::OK();
}

StatusOr<UDFType> RegistryInfo::GetUDFType(std::string_view name) {
  if (!funcs_.contains(name)) {
    return error::InvalidArgument("Could not find function '$0'.", name);
  }
  return funcs_[name];
}

absl::flat_hash_set<std::string> RegistryInfo::func_names() const {
  absl::flat_hash_set<std::string> func_names;
  for (const auto& pair : funcs_) {
    func_names.insert(pair.first);
  }
  return func_names;
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
    return error::InvalidArgument("Could not find UDF '$0' with arguments [$1].", name,
                                  absl::StrJoin(arg_data_type_strs, ","));
  }
  return udf->second;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
