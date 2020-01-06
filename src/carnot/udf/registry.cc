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

udfspb::UDFInfo UDTFRegistry::SpecToProto() const {
  udfspb::UDFInfo info;
  for (const auto& kv : map_) {
    auto* udtf_spec_pb = info.add_udtfs();
    const auto& udtf_def = kv.second;
    udtf_spec_pb->set_name(udtf_def->name());
    udtf_spec_pb->set_executor(udtf_def->executor());

    for (const auto& arg : udtf_def->init_arguments()) {
      auto new_arg = udtf_spec_pb->add_args();
      new_arg->set_name(std::string(arg.name()));
      new_arg->set_arg_type(arg.type());
      new_arg->set_semantic_type(arg.stype());
    }

    for (const auto& c : udtf_def->output_relation()) {
      auto new_c = udtf_spec_pb->mutable_relation()->add_columns();
      new_c->set_column_name(std::string(c.name()));
      new_c->set_column_type(c.type());
    }
  }
  return info;
}
}  // namespace udf
}  // namespace carnot
}  // namespace pl
