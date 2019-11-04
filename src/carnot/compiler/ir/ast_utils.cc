#include "src/carnot/compiler/ir/ast_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

StatusOr<std::vector<std::string>> ParseStringListIR(const ListIR* list_ir) {
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
