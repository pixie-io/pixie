#include <memory>
#include <string>

#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/flags_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/type_object.h"

namespace pl {
namespace carnot {
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
      std::bind(&FlagsObject::GetFlagHandler, this, std::placeholders::_1, std::placeholders::_2)));

  std::shared_ptr<FuncObject> register_flag_fn(
      new FuncObject(kCallMethodName, {"name", "type", "description", "default"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&FlagsObject::DefineFlagHandler, this, std::placeholders::_1,
                               std::placeholders::_2)));

  std::shared_ptr<FuncObject> parse_flags_fn(
      new FuncObject(kParseMethodName, {}, {}, false, false,
                     std::bind(&FlagsObject::ParseFlagsHandler, this, std::placeholders::_1,
                               std::placeholders::_2)));

  AddSubscriptMethod(subscript_fn);
  AddCallMethod(register_flag_fn);
  AddMethod(kParseMethodName, parse_flags_fn);
  return Status::OK();
}

StatusOr<QLObjectPtr> FlagsObject::DefineFlagHandler(const pypa::AstPtr& ast,
                                                     const ParsedArgs& args) {
  PL_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(args, "name"));
  std::string flag_name = name->str();

  if (parsed_flags_) {
    return CreateAstError(ast, "Could not add flag $0 after px.flags.parse() has been called",
                          flag_name);
  }

  QLObjectPtr type_arg = args.GetArg("type");
  if (type_arg->type() != QLObjectType::kType) {
    return CreateAstError(ast, "Expected type for px.flags argument 'type'");
  }
  auto type = std::static_pointer_cast<TypeObject>(type_arg);

  // TODO(nserrino): Give "default" a default value of None so that 'default' is an optional
  // argument.
  auto default_obj = args.GetArg("default");
  if (!default_obj->HasNode()) {
    return CreateAstError(ast, "Expected constant literal for px.flags argument 'default'");
  }

  auto defaultval = default_obj->node();

  // TODO(nserrino): Remove the requirement that defaults must be DataIR and support Expressions.
  // Using explicit matching here rather than GetArgAs to give a more helpful error message.
  if (!Match(defaultval, DataNode())) {
    return defaultval->CreateIRNodeError(
        "Value for 'default' in px.flags must be a constant literal, received $0",
        defaultval->type_string());
  }

  if (registered_flag_values_.contains(flag_name)) {
    return CreateAstError(ast, "Flag $0 already registered", flag_name);
  }

  DataIR* flag_value = nullptr;
  // Get the default if this query didn't receive a value for this flag.
  if (input_flag_values_.contains(flag_name)) {
    flag_value = input_flag_values_.at(flag_name);
  } else {
    flag_value = static_cast<DataIR*>(defaultval);
  }

  // TODO(nserrino): Support returning values provided by Flag protobufs, not just return defaults.
  if (!type->NodeMatches(flag_value).ok()) {
    return CreateAstError(ast, "For flag $0 expected type $1 but received type $2", flag_name,
                          IRNode::TypeString(type->ir_node_type()), flag_value->type_string());
  }
  registered_flag_values_[flag_name] = flag_value;
  return StatusOr(std::make_shared<NoneObject>());
}

bool FlagsObject::HasNonMethodAttribute(std::string_view name) const {
  return registered_flag_values_.contains(name);
}

StatusOr<QLObjectPtr> FlagsObject::GetAttributeImpl(const pypa::AstPtr& ast,
                                                    std::string_view flag_name) const {
  if (!parsed_flags_) {
    return CreateAstError(ast, "Cannot access flags before px.flags.parse() has been called",
                          flag_name);
  }
  if (!HasNonMethodAttribute(flag_name)) {
    return CreateAstError(ast, "Flag $0 not registered", flag_name);
  }
  DCHECK(registered_flag_values_.contains(flag_name));
  return ExprObject::Create(registered_flag_values_.at(flag_name));
}

StatusOr<QLObjectPtr> FlagsObject::GetFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args) {
  PL_ASSIGN_OR_RETURN(StringIR * flag_name, GetArgAs<StringIR>(args, "key"));
  return GetAttributeImpl(ast, flag_name->str());
}

StatusOr<QLObjectPtr> FlagsObject::ParseFlagsHandler(const pypa::AstPtr& ast,
                                                     const ParsedArgs& /*args*/) {
  if (parsed_flags_) {
    return CreateAstError(ast, "px.flags.parse() must only be called once");
  }
  parsed_flags_ = true;
  return StatusOr(std::make_shared<NoneObject>());
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
