#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/carnot/funcs/metadata/metadata_ops.h"
namespace pl {
namespace carnot {
namespace udfexporter {
udfspb::UDFInfo GetUDFProto() {
  auto registry = std::make_unique<udf::Registry>("udf_registry");
  builtins::RegisterBuiltinsOrDie(registry.get());
  funcs::metadata::RegisterMetadataOpsOrDie(registry.get());
  return registry->ToProto();
}

StatusOr<std::unique_ptr<compiler::RegistryInfo>> ExportUDFInfo() {
  udfspb::UDFInfo udf_proto = GetUDFProto();
  auto registry_info = std::make_unique<compiler::RegistryInfo>();
  PL_RETURN_IF_ERROR(registry_info->Init(udf_proto));
  return registry_info;
}

std::string FormatScalarUDF(const udfspb::ScalarUDFSpec& udf) {
  std::vector<std::string> arg_types;
  for (auto dt : udf.exec_arg_types()) {
    arg_types.push_back(types::DataType_Name(dt));
  }
  return absl::Substitute("$0($1) : $2", udf.name(), absl::StrJoin(arg_types, ","),
                          types::DataType_Name(udf.return_type()));
}

std::string FormatUDA(const udfspb::UDASpec& uda) {
  std::vector<std::string> arg_types;
  for (auto dt : uda.update_arg_types()) {
    arg_types.push_back(types::DataType_Name(dt));
  }
  return absl::Substitute("$0($1) : $2", uda.name(), absl::StrJoin(arg_types, ","),
                          types::DataType_Name(uda.finalize_type()));
}

std::string UDFProtoFormatString() {
  const udfspb::UDFInfo& udf_proto = GetUDFProto();
  std::vector<std::string> scalar_udfs_strs;
  std::vector<std::string> uda_strs;

  for (const auto& udf : udf_proto.scalar_udfs()) {
    scalar_udfs_strs.push_back(FormatScalarUDF(udf));
  }
  for (const auto& uda : udf_proto.udas()) {
    uda_strs.push_back(FormatUDA(uda));
  }

  return absl::Substitute("Map functions:\n\t$0\nAggregate functions:\n\t$1",
                          absl::StrJoin(scalar_udfs_strs, "\n\t"), absl::StrJoin(uda_strs, "\n\t"));
}
}  // namespace udfexporter
}  // namespace carnot
}  // namespace pl
