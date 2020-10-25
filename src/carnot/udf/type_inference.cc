#include "src/carnot/udf/type_inference.h"

#include "src/common/base/hash_utils.h"

namespace pl {
namespace carnot {
namespace udf {

size_t ExplicitRule::Hash() const {
  auto int_hash = std::hash<int>{};
  size_t hash = 0;
  hash = ::pl::HashCombine(hash, int_hash(static_cast<int>(udf_exec_type_)));
  hash = ::pl::HashCombine(hash, int_hash(static_cast<int>(out_type_)));
  for (const auto& arg_type : init_arg_types_) {
    hash = ::pl::HashCombine(hash, int_hash(static_cast<int>(arg_type)));
  }
  for (const auto& arg_type : exec_or_update_types_) {
    hash = ::pl::HashCombine(hash, int_hash(static_cast<int>(arg_type)));
  }
  return hash;
}

bool ExplicitRule::Equals(const ExplicitRulePtr& other) const {
  return other->udf_exec_type_ == udf_exec_type_ && other->out_type_ == out_type_ &&
         other->init_arg_types_ == init_arg_types_ &&
         other->exec_or_update_types_ == exec_or_update_types_;
}
void ExplicitRule::ToProto(const std::string& name, udfspb::SemanticInferenceRule* rule) const {
  rule->set_name(name);
  rule->set_udf_exec_type(udf_exec_type_);
  rule->set_output_type(out_type_);
  for (const auto& init_arg_type : init_arg_types_) {
    rule->add_init_arg_types(init_arg_type);
  }
  for (const auto& arg_type : exec_or_update_types_) {
    switch (udf_exec_type_) {
      case udfspb::SCALAR_UDF:
        rule->add_exec_arg_types(arg_type);
        break;
      case udfspb::UDA:
        rule->add_update_arg_types(arg_type);
        break;
      default:
        CHECK(false) << "Unknown UDFExecType";
        return;
    }
  }
}
}  // namespace udf
}  // namespace carnot
}  // namespace pl
