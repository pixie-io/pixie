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
    auto key = RegistryKey(uda.name(), arg_types, uda.finalize_type());
    uda_map_[key] = true;
  }

  for (auto udf : info.scalar_udfs()) {
    std::vector<types::DataType> arg_types;
    arg_types.reserve(udf.exec_arg_types_size());

    for (int64_t i = 0; i < udf.exec_arg_types_size(); i++) {
      arg_types.push_back(udf.exec_arg_types(i));
    }
    auto key = RegistryKey(udf.name(), arg_types, udf.return_type());
    udf_map_[key] = true;
  }

  return Status::OK();
}

bool RegistryInfo::UDAExists(std::string name, std::vector<types::DataType> update_arg_types,
                             types::DataType return_type) {
  auto uda = uda_map_.find(RegistryKey(name, update_arg_types, return_type));
  return uda != uda_map_.end();
}

bool RegistryInfo::UDFExists(std::string name, std::vector<types::DataType> exec_arg_types,
                             types::DataType return_type) {
  auto udf = udf_map_.find(RegistryKey(name, exec_arg_types, return_type));
  return udf != udf_map_.end();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
