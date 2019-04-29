#include "src/carnot/udf_exporter/udf_exporter.h"
namespace pl {
namespace carnot {
namespace udfexporter {
StatusOr<std::unique_ptr<compiler::RegistryInfo>> ExportUDFInfo() {
  auto scalar_udf_registry = std::make_unique<udf::ScalarUDFRegistry>("udf_registry");
  auto uda_registry = std::make_unique<udf::UDARegistry>("uda_registry");
  builtins::RegisterBuiltinsOrDie(scalar_udf_registry.get());
  builtins::RegisterBuiltinsOrDie(uda_registry.get());
  auto udf_proto =
      udf::RegistryInfoExporter().Registry(*uda_registry).Registry(*scalar_udf_registry).ToProto();
  auto registry_info = std::make_unique<compiler::RegistryInfo>();
  PL_RETURN_IF_ERROR(registry_info->Init(udf_proto));
  return registry_info;
}
}  // namespace udfexporter
}  // namespace carnot
}  // namespace pl
