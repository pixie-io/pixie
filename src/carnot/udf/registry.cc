/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/carnot/udf/registry.h"

namespace px {
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
    PX_UNUSED(name);
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
  for (const auto& [name, explicit_rule_set] : semantic_type_rules_) {
    for (const auto& explicit_rule : explicit_rule_set) {
      explicit_rule->ToProto(name, info.add_semantic_type_rules());
    }
  }
  return info;
}

void Registry::ToProto(const ScalarUDFDefinition& def, udfspb::ScalarUDFSpec* spec) {
  const auto& exec_arguments = def.exec_arguments();
  *spec->mutable_exec_arg_types() = {exec_arguments.begin(), exec_arguments.end()};
  const auto& init_arguments = def.init_arguments();
  *spec->mutable_init_arg_types() = {init_arguments.begin(), init_arguments.end()};
  spec->set_executor(def.executor());
  spec->set_return_type(def.exec_return_type());
  spec->set_name(def.name());
  spec->set_executor(def.executor());
}

void Registry::ToProto(const UDADefinition& def, udfspb::UDASpec* spec) {
  const auto& update_argument = def.update_arguments();
  *spec->mutable_update_arg_types() = {update_argument.begin(), update_argument.end()};
  const auto& init_arguments = def.init_arguments();
  *spec->mutable_init_arg_types() = {init_arguments.begin(), init_arguments.end()};
  spec->set_finalize_type(def.finalize_return_type());
  spec->set_name(def.name());
  spec->set_supports_partial(def.supports_partial());
}

namespace {
// PX_CARNOT_UPDATE_FOR_NEW_TYPES.
template <types::DataType dt, size_t S = sizeof(dt)>
void DefaultToScalarValue(const UDTFArg&, planpb::ScalarValue*) {
  static_assert(S == 0, "This template func must be specialized for type");
}

template <>
void DefaultToScalarValue<types::BOOLEAN>(const UDTFArg& arg, planpb::ScalarValue* out) {
  out->set_bool_value(arg.GetDefaultValue<types::BOOLEAN>().val);
}

template <>
void DefaultToScalarValue<types::INT64>(const UDTFArg& arg, planpb::ScalarValue* out) {
  out->set_int64_value(arg.GetDefaultValue<types::INT64>().val);
}

template <>
void DefaultToScalarValue<types::TIME64NS>(const UDTFArg& arg, planpb::ScalarValue* out) {
  out->set_time64_ns_value(arg.GetDefaultValue<types::TIME64NS>().val);
}

template <>
void DefaultToScalarValue<types::FLOAT64>(const UDTFArg& arg, planpb::ScalarValue* out) {
  out->set_float64_value(arg.GetDefaultValue<types::FLOAT64>().val);
}

template <>
void DefaultToScalarValue<types::UINT128>(const UDTFArg& arg, planpb::ScalarValue* out) {
  auto out_val = out->mutable_uint128_value();
  auto casted_arg = arg.GetDefaultValue<types::UINT128>();

  out_val->set_high(casted_arg.High64());
  out_val->set_high(casted_arg.Low64());
}

template <>
void DefaultToScalarValue<types::STRING>(const UDTFArg& arg, planpb::ScalarValue* out) {
  out->set_string_value(std::string(arg.GetDefaultValue<types::STRING>()));
}
}  // namespace

void Registry::ToProto(const UDTFDefinition& def, udfspb::UDTFSourceSpec* spec) {
  spec->set_name(def.name());
  spec->set_executor(def.executor());

  for (const auto& arg : def.init_arguments()) {
    auto new_arg = spec->add_args();
    new_arg->set_name(std::string(arg.name()));
    new_arg->set_arg_type(arg.type());
    new_arg->set_semantic_type(arg.stype());
    if (arg.has_default_val()) {
      auto arg_type = arg.type();
#define TYPE_CASE(_dt_) DefaultToScalarValue<_dt_>(arg, new_arg->mutable_default_value());

      PX_SWITCH_FOREACH_DATATYPE(arg_type, TYPE_CASE);
#undef TYPE_CASE
    }
  }

  for (const auto& c : def.output_relation()) {
    auto new_c = spec->mutable_relation()->add_columns();
    new_c->set_column_name(std::string(c.name()));
    new_c->set_column_type(c.type());
    new_c->set_column_semantic_type(c.stype());
  }
}

std::string Registry::DebugString() const {
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
  PX_ASSIGN_OR_RETURN(auto def, GetDefinition(name, registry_arg_types));
  if (def->kind() != UDFDefinitionKind::kUDTF) {
    return error::NotFound("'$0' is not a UDTF", name);
  }
  return static_cast<UDTFDefinition*>(def);
}

StatusOr<UDADefinition*> Registry::GetUDADefinition(
    const std::string& name, const std::vector<types::DataType>& registry_arg_types) const {
  PX_ASSIGN_OR_RETURN(auto def, GetDefinition(name, registry_arg_types));
  if (def->kind() != UDFDefinitionKind::kUDA) {
    return error::NotFound("'$0' is not a UDA", name);
  }
  return static_cast<UDADefinition*>(def);
}

StatusOr<ScalarUDFDefinition*> Registry::GetScalarUDFDefinition(
    const std::string& name, const std::vector<types::DataType>& registry_arg_types) const {
  PX_ASSIGN_OR_RETURN(auto def, GetDefinition(name, registry_arg_types));
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
}  // namespace px
