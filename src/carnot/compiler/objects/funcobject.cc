#include "src/carnot/compiler/objects/funcobject.h"
#include "src/carnot/compiler/parser/parser.h"

namespace pl {
namespace carnot {
namespace compiler {

FuncObject::FuncObject(const std::string_view name, const std::vector<std::string>& arguments,
                       const absl::flat_hash_map<std::string, DefaultType>& defaults,
                       bool has_kwargs, FunctionType impl)
    : QLObject(FuncType),
      name_(name),
      arguments_(arguments),
      defaults_(defaults),
      impl_(impl),
      has_kwargs_(has_kwargs) {
#if DCHECK_IS_ON()
  for (const auto& arg : defaults) {
    DCHECK(std::find(arguments.begin(), arguments.end(), arg.first) != arguments.end())
        << absl::Substitute("Default argument '$0' does not match an actual argument.", arg.first);
  }
#endif
}

StatusOr<QLObjectPtr> FuncObject::Call(const ArgMap& args, const pypa::AstPtr& ast,
                                       ASTVisitor* ast_visitor) {
  PL_ASSIGN_OR_RETURN(ParsedArgs parsed_args, PrepareArgs(args, ast, ast_visitor));
  return impl_(ast, parsed_args);
}

StatusOr<ParsedArgs> FuncObject::PrepareArgs(const ArgMap& args, const pypa::AstPtr& ast,
                                             ASTVisitor* ast_visitor) {
  // Iterate through the arguments and place them in.
  ParsedArgs parsed_args;

  // If the number of args is greater than all args, then we throw an error.
  int64_t input_nargs = static_cast<int64_t>(args.args.size());
  if (input_nargs > NumArgs()) {
    std::string err_msg = absl::Substitute("$0() takes $1 arguments but $2 were given.", name(),
                                           NumArgs(), args.args.size());
    return CreateAstError(ast, err_msg);
  }

  for (const auto& [idx, node] : Enumerate(args.args)) {
    std::string arg_name = arguments_[idx];
    parsed_args.AddArg(arg_name, node);
  }

  absl::flat_hash_set<std::string> missing_args(arguments_.begin() + input_nargs, arguments_.end());
  // Parse through the keyword args.
  for (const auto& [arg, node] : args.kwargs) {
    if (parsed_args.HasArgOrKwarg(arg)) {
      std::string err_msg =
          absl::Substitute("$0() got multiple values for argument '$1'", name(), arg);
      return CreateAstError(ast, err_msg);
    }
    if (!missing_args.contains(arg)) {
      if (!has_kwargs_) {
        std::string err_msg =
            absl::Substitute("$0() got an unexpected keyword argument '$1'", name(), arg);
        return CreateAstError(ast, err_msg);
      }
      parsed_args.AddKwarg(arg, node);
    } else {
      parsed_args.AddArg(arg, node);
      missing_args.erase(arg);
    }
  }

  // Substitute defaults for missing args. Anything else is a missing positional argument.
  absl::flat_hash_set<std::string> missing_pos_args;
  for (const std::string& arg : missing_args) {
    if (!HasDefault(arg)) {
      missing_pos_args.emplace(arg);
      continue;
    }
    PL_ASSIGN_OR_RETURN(IRNode * default_node, GetDefault(arg, ast_visitor));
    parsed_args.AddArg(arg, default_node);
  }

  // If missing positional arguments.
  if (missing_pos_args.size()) {
    std::string err_msg =
        absl::Substitute("$0() missing $1 required positional arguments $2", name(),
                         missing_pos_args.size(), FormatArguments(missing_pos_args));
    return CreateAstError(ast, err_msg);
  }
  return parsed_args;
}

bool FuncObject::HasDefault(const std::string& arg) {
  return defaults_.find(arg) != defaults_.end();
}

StatusOr<IRNode*> FuncObject::GetDefault(std::string_view arg, ASTVisitor* ast_visitor) {
  //  Check if the argument exists among the defaults.
  if (!defaults_.contains(arg)) {
    return error::InvalidArgument("");
  }
  const std::string& arg_str = defaults_.find(arg)->second;
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(arg_str));

  // TODO(philkuz) remove compiler state argument as it is no longer in use.
  return ast_visitor->ProcessSingleExpressionModule(ast);
}

std::string FuncObject::FormatArguments(const absl::flat_hash_set<std::string> args) {
  // Joins the argument names by commas and surrounds each arg with single quotes.
  return absl::StrJoin(args, ",", [](std::string* out, const std::string& arg) {
    absl::StrAppend(out, absl::Substitute("'$0'", arg));
  });
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
