#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace udf {

udfspb::UDFInfo ScalarUDFRegistry::SpecToProto() const {
  udfspb::UDFInfo info;
  for (const auto& kv : map_) {
    auto* udf_spec_pb = info.add_scalar_udfs();
    const auto& udf_def = kv.second;
    const auto& exec_arguments = udf_def->exec_arguments();
    *udf_spec_pb->mutable_exec_arg_types() = {exec_arguments.begin(), exec_arguments.end()};
    udf_spec_pb->set_return_type(udf_def->exec_return_type());
    udf_spec_pb->set_name(udf_def->name());
  }
  return info;
}

udfspb::UDFInfo UDARegistry::SpecToProto() const {
  udfspb::UDFInfo info;
  for (const auto& kv : map_) {
    auto* udf_spec_pb = info.add_udas();
    const auto& udf_def = kv.second;
    const auto& update_argument = udf_def->update_arguments();
    *udf_spec_pb->mutable_update_arg_types() = {update_argument.begin(), update_argument.end()};
    udf_spec_pb->set_finalize_type(udf_def->finalize_return_type());
    udf_spec_pb->set_name(udf_def->name());
  }
  return info;
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
