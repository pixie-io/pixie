#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace udf {

std::string RegistryKey::DebugString() const {
  std::vector<std::string> name_vector;
  for (auto const& type : registry_arg_types_) {
    name_vector.push_back(types::DataType_Name(type));
  }
  return absl::Substitute("$0($1)", name_, absl::StrJoin(name_vector, ","));
}

bool RegistryKey::operator<(const RegistryKey& lhs) const {
  if (name_ == lhs.name_) {
    return registry_arg_types_ < lhs.registry_arg_types_;
  }
  return name_ < lhs.name_;
}

udfspb::UDFInfo Registry::ToProto() {
  udfspb::UDFInfo info;
  for (const auto& [name, def] : map_) {
    PL_UNUSED(name);
    switch (def->kind()) {
      case UDFDefinitionKind::kScalarUDF:
        ToProto(*static_cast<ScalarUDFDefinition*>(def.get()), info.add_scalar_udfs());
        break;
      case UDFDefinitionKind::kUDA:
        ToProto(*static_cast<UDADefinition*>(def.get()), info.add_udas());
        break;
      case UDFDefinitionKind::kUDTF:
        ToProto(*static_cast<UDTFDefinition*>(def.get()), info.add_udtfs());
        break;
    }
  }
  return info;
}

void Registry::ToProto(const ScalarUDFDefinition& def, udfspb::ScalarUDFSpec* spec) {
  const auto& exec_arguments = def.exec_arguments();
  *spec->mutable_exec_arg_types() = {exec_arguments.begin(), exec_arguments.end()};
  spec->set_return_type(def.exec_return_type());
  spec->set_name(def.name());
}

void Registry::ToProto(const UDADefinition& def, udfspb::UDASpec* spec) {
  const auto& update_argument = def.update_arguments();
  *spec->mutable_update_arg_types() = {update_argument.begin(), update_argument.end()};
  spec->set_finalize_type(def.finalize_return_type());
  spec->set_name(def.name());
}

void Registry::ToProto(const UDTFDefinition& def, udfspb::UDTFSourceSpec* spec) {
  spec->set_name(def.name());
  spec->set_executor(def.executor());

  for (const auto& arg : def.init_arguments()) {
    auto new_arg = spec->add_args();
    new_arg->set_name(std::string(arg.name()));
    new_arg->set_arg_type(arg.type());
    new_arg->set_semantic_type(arg.stype());
  }

  for (const auto& c : def.output_relation()) {
    auto new_c = spec->mutable_relation()->add_columns();
    new_c->set_column_name(std::string(c.name()));
    new_c->set_column_type(c.type());
  }
}

std::string Registry::DebugString() {
  std::string debug_string;
  debug_string += absl::Substitute("udf::Registry: $0\n", name_);
  for (const auto& entry : map_) {
    // TODO(zasgar): add arguments as well. Future Diff.
    debug_string += entry.first.name() + "\n";
  }
  return debug_string;
}

StatusOr<UDTFDefinition*> Registry::GetUDTFDefinition(
    const std::string& name, const std::vector<types::DataType>& registry_arg_types) const {
  PL_ASSIGN_OR_RETURN(auto def, GetDefinition(name, registry_arg_types));
  if (def->kind() != UDFDefinitionKind::kUDTF) {
    return error::NotFound("'$0' is not a UDTF", name);
  }
  return static_cast<UDTFDefinition*>(def);
}

StatusOr<UDADefinition*> Registry::GetUDADefinition(
    const std::string& name, const std::vector<types::DataType>& registry_arg_types) const {
  PL_ASSIGN_OR_RETURN(auto def, GetDefinition(name, registry_arg_types));
  if (def->kind() != UDFDefinitionKind::kUDA) {
    return error::NotFound("'$0' is not a UDA", name);
  }
  return static_cast<UDADefinition*>(def);
}

StatusOr<ScalarUDFDefinition*> Registry::GetScalarUDFDefinition(
    const std::string& name, const std::vector<types::DataType>& registry_arg_types) const {
  PL_ASSIGN_OR_RETURN(auto def, GetDefinition(name, registry_arg_types));
  if (def->kind() != UDFDefinitionKind::kScalarUDF) {
    return error::NotFound("'$0' is not a ScalarUDF", name);
  }
  return static_cast<ScalarUDFDefinition*>(def);
}

StatusOr<UDFDefinition*> Registry::GetDefinition(
    const std::string& name, const std::vector<types::DataType>& registry_arg_types) const {
  auto key = RegistryKey(name, registry_arg_types);
  auto it = map_.find(key);
  if (it == map_.end()) {
    return error::NotFound("No UDF matching $0 found.", key.DebugString());
  }
  return it->second.get();
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
