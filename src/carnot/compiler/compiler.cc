#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/string_reader.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {
StatusOr<carnotpb::Plan> Compiler::Compile(const std::string& query,
                                           CompilerState* compiler_state) {
  PL_UNUSED(compiler_state);
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query));
  return IRToLogicalPlan(*ir);
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query) {
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTWalker ast_walker(ir);

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  pypa::Lexer lexer(std::make_unique<StringReader>(query));

  Status result;
  if (pypa::parse(lexer, ast, symbols, options)) {
    result = ast_walker.ProcessModuleNode(ast);
  } else {
    result = error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }
  PL_RETURN_IF_ERROR(result);
  return ir;
}

StatusOr<carnotpb::Plan> Compiler::IRToLogicalPlan(const IR& ir) {
  auto plan = carnotpb::Plan();
  // TODO(michelle) For M1.5 , we'll only handle plans with a single plan fragment. In the future we
  // will need to update this to loop through all plan fragments.
  auto plan_dag = plan.mutable_dag();
  auto plan_dag_node = plan_dag->add_nodes();
  plan_dag_node->set_id(1);

  auto plan_fragment = plan.add_nodes();
  plan_fragment->set_id(1);
  auto plan_fragment_dag = plan_fragment->mutable_dag();

  IRWalker()
      .OnMemorySink([&](const auto& mem_sink) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, mem_sink);
      })
      .OnMemorySource([&](const auto& mem_src) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, mem_src);
      })
      .OnMap([&](const auto& map) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, map);
      })
      .OnAgg([&](const auto& agg) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, agg);
      })
      .Walk(ir);
  return plan;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
