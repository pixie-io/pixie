#include "src/carnot/compiler/ir/ast_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

StatusOr<std::vector<std::string>> ParseStringsFromCollection(const CollectionIR* list_ir) {
  std::vector<std::string> out_vector;
  for (size_t idx = 0; idx < list_ir->children().size(); ++idx) {
    IRNode* child_ir = list_ir->children()[idx];
    if (!Match(child_ir, String())) {
      return child_ir->CreateIRNodeError("The elements of the list must be Strings, not '$0'.",
                                         child_ir->type_string());
    }
    StringIR* string_node = static_cast<StringIR*>(child_ir);
    out_vector.push_back(string_node->str());
  }
  return out_vector;
}

std::string GetAstTypeName(pypa::AstType type) {
  std::vector<std::string> type_names = {
#undef PYPA_AST_TYPE
#define PYPA_AST_TYPE(X) #X,
// NOLINTNEXTLINE(build/include_order).
#include <pypa/ast/ast_type.inl>
#undef PYPA_AST_TYPE
  };
  DCHECK(type_names.size() > static_cast<size_t>(type));
  return absl::Substitute("$0", type_names[static_cast<int>(type)]);
}

std::string GetNameAsString(const pypa::AstPtr& node) { return PYPA_PTR_CAST(Name, node)->id; }

StatusOr<std::string> GetStrAstValue(const pypa::AstPtr& ast) {
  if (ast->type != pypa::AstType::Str) {
    return CreateAstError(ast, "Expected string type. Got $0", GetAstTypeName(ast->type));
  }
  return PYPA_PTR_CAST(Str, ast)->value;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
