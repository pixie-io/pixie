#include <memory>
#include <string>

#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/flags_object.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<absl::flat_hash_map<std::string, DataIR*>> ParseFlagValues(IR* ir,
                                                                    const FlagValues& flag_values) {
  absl::flat_hash_map<std::string, DataIR*> map;
  for (const auto& flag : flag_values) {
    auto name = absl::Substitute("flag $0", flag.flag_name());
    PL_ASSIGN_OR_RETURN(auto parsed_value, DataIR::FromProto(ir, name, flag.flag_value()));
    if (map.contains(flag.flag_name())) {
      return error::InvalidArgument("Received duplicate values for $0", name);
    }
    map[flag.flag_name()] = parsed_value;
  }
  return map;
}

Status FlagsObject::Init(const FlagValues& flag_values) {
  PL_ASSIGN_OR_RETURN(input_flag_values_, ParseFlagValues(ir_graph_, flag_values));

  std::shared_ptr<FuncObject> subscript_fn(new FuncObject(
      kSubscriptMethodName, {"key"}, {}, /* has_variable_len_args */ false,
      /* has_variable_len_kwargs */ false,
      std::bind(&FlagsObject::GetFlagHandler, this, std::placeholders::_1, std::placeholders::_2),
      ast_visitor()));

  std::shared_ptr<FuncObject> register_flag_fn(new FuncObject(
      kCallMethodName, {"name", "type", "description", "default"}, {{"default", "None"}},
      /* has_variable_len_args */ false,
      /* has_variable_len_kwargs */ false,
      std::bind(&FlagsObject::DefineFlagHandler, this, std::placeholders::_1,
                std::placeholders::_2),
      ast_visitor()));

  std::shared_ptr<FuncObject> parse_flags_fn(
      new FuncObject(kParseMethodName, {}, {}, false, false,
                     std::bind(&FlagsObject::ParseFlagsHandler, this, std::placeholders::_1,
                               std::placeholders::_2),
                     ast_visitor()));

  AddSubscriptMethod(subscript_fn);
  AddCallMethod(register_flag_fn);
  AddMethod(kParseMethodName, parse_flags_fn);
  return Status::OK();
}

bool FlagsObject::HasFlag(std::string_view flag_name) const {
  return flag_types_.contains(flag_name);
}

StatusOr<QLObjectPtr> FlagsObject::DefineFlagHandler(const pypa::AstPtr& ast,
                                                     const ParsedArgs& args) {
  // Parse name and description
  PL_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(args, "name"));
  std::string flag_name = name->str();
  PL_ASSIGN_OR_RETURN(StringIR * desc, GetArgAs<StringIR>(args, "description"));
  std::string description = desc->str();

  // Parse flag type
  QLObjectPtr type_arg = args.GetArg("type");
  if (type_arg->type() != QLObjectType::kType) {
    return CreateAstError(ast, "Expected type for px.flags argument 'type'");
  }
  auto type = std::static_pointer_cast<TypeObject>(type_arg);
  // Parse flag default
  auto default_obj = args.GetArg("default");

  // Check error cases
  if (parsed_flags_) {
    return CreateAstError(ast, "Could not add flag $0 after px.flags.parse() has been called",
                          flag_name);
  }
  if (HasFlag(flag_name)) {
    return CreateAstError(ast, "Flag $0 already registered", flag_name);
  }

  DataIR* default_val = nullptr;
  if (default_obj->type() == QLObjectType::kExpr) {
    // Verify types
    if (!Match(default_obj->node(), DataNode())) {
      return default_obj->node()->CreateIRNodeError(
          "Value for 'default' in px.flags must be a constant literal, received $0",
          default_obj->node()->type_string());
    }
    if (!type->NodeMatches(default_obj->node()).ok()) {
      return CreateAstError(
          ast, "For default value of flag $0 expected type $1 but received type $2", flag_name,
          IRNode::TypeString(type->ir_node_type()), default_obj->node()->type_string());
    }
    default_val = static_cast<DataIR*>(default_obj->node());
  } else if (default_obj->type() == QLObjectType::kNone) {
    // Ok, passthrough
  } else {
    return CreateAstError(ast, "Unexpected type $0 for default of flag $1", default_obj->name(),
                          flag_name);
  }

  if (input_flag_values_.contains(flag_name) &&
      !type->NodeMatches(input_flag_values_[flag_name]).ok()) {
    return CreateAstError(ast, "For input value of flag $0 expected type $1 but received type $2",
                          flag_name, IRNode::TypeString(type->ir_node_type()),
                          input_flag_values_[flag_name]->type_string());
  }

  // Assign values
  if (default_val) {
    default_flag_values_[flag_name] = default_val;
  }
  flag_types_[flag_name] = type;
  flag_descriptions_[flag_name] = description;
  return StatusOr(std::make_shared<NoneObject>(ast_visitor()));
}

bool FlagsObject::HasNonMethodAttribute(std::string_view name) const { return HasFlag(name); }

StatusOr<QLObjectPtr> FlagsObject::GetAttributeImpl(const pypa::AstPtr& ast,
                                                    std::string_view flag_name) const {
  if (!parsed_flags_) {
    return CreateAstError(ast, "Cannot access flags before px.flags.parse() has been called",
                          flag_name);
  }
  return QLObject::GetAttributeImpl(ast, flag_name);
}

StatusOr<QLObjectPtr> FlagsObject::GetFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args) {
  PL_ASSIGN_OR_RETURN(StringIR * flag_name, GetArgAs<StringIR>(args, "key"));
  return GetAttribute(ast, flag_name->str());
}

StatusOr<QLObjectPtr> FlagsObject::ParseFlagsHandler(const pypa::AstPtr& ast,
                                                     const ParsedArgs& /*args*/) {
  if (parsed_flags_) {
    return CreateAstError(ast, "px.flags.parse() must only be called once");
  }

  for (const auto& [flag_name, flag_value] : input_flag_values_) {
    PL_UNUSED(flag_value);
    if (!flag_types_.contains(flag_name)) {
      return CreateAstError(ast, "Received flag $0 which was not registered in script", flag_name);
    }
  }

  // Assign an attribute to each flag
  for (const auto& [flag_name, flag_type] : flag_types_) {
    PL_UNUSED(flag_type);
    DataIR* assign_value = nullptr;
    if (input_flag_values_.contains(flag_name)) {
      assign_value = input_flag_values_.at(flag_name);
    } else if (default_flag_values_.contains(flag_name)) {
      assign_value = default_flag_values_.at(flag_name);
    } else if (default_zero_values_) {
      PL_ASSIGN_OR_RETURN(assign_value, DataIR::ZeroValueForType(
                                            ir_graph_, flag_types_.at(flag_name)->ir_node_type()));
    } else {
      return CreateAstError(ast, "Did not receive a value for required flag $0 (type $1)",
                            flag_name,
                            IRNode::TypeString(flag_types_.at(flag_name)->ir_node_type()));
    }

    PL_ASSIGN_OR_RETURN(auto obj, ExprObject::Create(assign_value, ast_visitor()));
    PL_RETURN_IF_ERROR(AssignAttribute(flag_name, obj));
  }

  parsed_flags_ = true;
  return StatusOr(std::make_shared<NoneObject>(ast_visitor()));
}

StatusOr<plannerpb::QueryFlagsSpec> FlagsObject::GetAvailableFlags(const pypa::AstPtr& ast) const {
  if (!parsed_flags_ && flag_types_.size()) {
    return CreateAstError(
        ast, "Flags registered with px.flags, but px.flags.parse() has not been called");
  }
  plannerpb::QueryFlagsSpec flags;
  for (const auto& [flag_name, description] : flag_descriptions_) {
    DCHECK(flag_types_.contains(flag_name));
    auto type = flag_types_.at(flag_name);

    auto flag = flags.add_flags();
    flag->set_name(flag_name);
    flag->set_description(flag_descriptions_.at(flag_name));
    flag->set_data_type(DataIR::DataType(type->ir_node_type()));
    // TODO(philkuz, nserrino): Change this once semantic types are added to QL.
    flag->set_semantic_type(types::SemanticType::ST_NONE);

    if (default_flag_values_.contains(flag_name)) {
      PL_RETURN_IF_ERROR(
          default_flag_values_.at(flag_name)->ToProto(flag->mutable_default_value()));
    }
  }
  return flags;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
