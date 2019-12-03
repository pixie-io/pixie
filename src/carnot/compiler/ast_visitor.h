#pragma once
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include "src/carnot/compiler/ast/ast_visitor.h"
#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/ir/ast_utils.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/objects/dataframe.h"

namespace pl {
namespace carnot {
namespace compiler {

using VarTable = std::unordered_map<std::string, QLObjectPtr>;

#define PYPA_PTR_CAST(TYPE, VAL) \
  std::static_pointer_cast<typename pypa::AstTypeByID<pypa::AstType::TYPE>::Type>(VAL)

#define PYPA_CAST(TYPE, VAL) static_cast<AstTypeByID<AstType::TYPE>::Type&>(VAL)

struct OperatorContext {
  const std::vector<OperatorIR*> parent_ops;
  std::string operator_name;
  // A list of the names of dataframes that can be accessed in this operator.
  const std::vector<std::string> referenceable_dataframes;
  OperatorContext(const std::vector<OperatorIR*>& parents, std::string op_name)
      : OperatorContext(parents, op_name, {}) {}
  OperatorContext(const std::vector<OperatorIR*>& parents, std::string op_name,
                  const std::vector<std::string>& dfs)
      : parent_ops(parents), operator_name(op_name), referenceable_dataframes(dfs) {}
  OperatorContext(const std::vector<OperatorIR*>& parents, OperatorIR* op)
      : parent_ops(parents), operator_name(op->type_string()) {}
};

class ASTVisitorImpl : public ASTVisitor {
 public:
  static StatusOr<std::shared_ptr<ASTVisitorImpl>> Create(IR* graph, CompilerState* compiler_state);
  /**
   * @brief The entry point into traversal as the root AST is a module.
   *
   * @param node: the ptr to the ast node.
   * @return Status
   */
  Status ProcessModuleNode(const pypa::AstModulePtr& m) override;

  /**
   * @brief Processes a single expression into an IRNode. If the parameter has more than one
   * line or contains a module we can't process, then it will error out.
   *
   * @param m the poitner to the parsed ast module.
   * @param op_context the operator context to operate with.
   * @return StatusOr<IRNode*> the graph representation of the expression, or an error if the
   * operator fails.
   */
  StatusOr<IRNode*> ProcessSingleExpressionModule(const pypa::AstModulePtr& m) override;

  /**
   * @brief Parses and processes out a single expression in the form of an IRNode.
   *
   * @param str the input string
   * @return StatusOr<IRNode*> the IR of the expression or an error if something fails during
   * processing.
   */
  StatusOr<IRNode*> ParseAndProcessSingleExpression(std::string_view str) override;

  IR* ir_graph() const { return ir_graph_; }

  // Constants for the run-time (UDF) and compile-time fn prefixes.
  inline static constexpr char kRunTimeFuncPrefix[] = "pl";
  inline static constexpr char kCompileTimeFuncPrefix[] = "plc";

  // Constant for the metadata attribute keyword.
  inline static constexpr char kMDKeyword[] = "attr";

  // Constants for operators in the query language.

  // Constant for the modules.
  inline static constexpr char kPLModuleObjName[] = "pl";

  // Reserved column names.
  inline static constexpr char kTimeConstantColumnName[] = "time_";

 private:
  /**
   * @brief Construct a new ASTWalker object.
   * This constructor will be used at the top level.
   *
   * @param ir_graph
   */
  ASTVisitorImpl(IR* ir_graph, CompilerState* compiler_state)
      : ir_graph_(ir_graph), compiler_state_(compiler_state) {}

  Status Init();

  /**
   * @brief ProcessArgs traverses an arg_ast tree, converts the expressions into IR and then returns
   * a data structure of positional and keyword arguments representing the arguments in IR.
   *
   * @param call_ast the ast node that calls the arguments.
   * @param op_context the op context used to process args.
   * @return StatusOr<ArgMap> a mapping of arguments (positional and kwargs) to the resulting
   * IRNode.
   */
  StatusOr<ArgMap> ProcessArgs(const pypa::AstCallPtr& call_ast, const OperatorContext& op_context);

  /**
   * @brief ProcessArgs traverses an arg_ast tree, converts the expressions into IR and then returns
   * a data structure of positional and keyword arguments representing the arguments in IR.
   *
   * @param call_ast the ast node that calls the arguments.
   * @return StatusOr<ArgMap> a mapping of arguments (positional and kwargs) to the resulting
   * IRNode.
   */
  StatusOr<ArgMap> ProcessArgs(const pypa::AstCallPtr& call_ast);

  /**
   * @brief ProcessExprStmtNode handles full lines that are expression statements.
   * ie in the following lines
   *  1: a =From(...)
   *  2: a.Range(...)
   * Line 1 will be wrapped in an AstAssignNode
   * Line 2 will be wrapped in an AstExpressionStatementNode.
   *
   * The entirety of line 2 is the expression statement and will be handled by this function.
   *
   * @param node
   * @return Status: whether the expression could be parsed or not.
   */
  Status ProcessExprStmtNode(const pypa::AstExpressionStatementPtr& node);

  /**
   * @brief ProcessSubscriptMapAssignment handles lines where a map is assigned via a statement like
   *  1: a['foo'] = a['bar'] * 2 + a['abc']
   *   *
   * @param assign_node the target of the assignment
   * @param expr_node the expression of the assignment
   * @return Status whether the assignment worked or not.
   */
  Status ProcessSubscriptMapAssignment(const pypa::AstSubscriptPtr& assign_node,
                                       const pypa::AstPtr& expr_node);

  /**
   * @brief ProcessAssignNode handles lines where an expression is assigned to a value.
   * ie in the following lines
   *  1: a =From(...)
   *  2: a.Range(...)
   * Line 1 will be wrapped in an AstAssignNode
   * Line 2 will be wrapped in an AstExpressionStatementNode.
   *
   * The entirety of line 1 is the assign statement and will be handled by this function.
   *
   * @param node
   * @return Status whether the assignment worked or not.
   */
  Status ProcessAssignNode(const pypa::AstAssignPtr& node);

  /**
   * @brief Gets the function name out of the call node into a string.
   *
   * @param call ptr ast node.
   * @return StatusOr<std::string> the string name of the function.
   */
  StatusOr<std::string> GetFuncName(const pypa::AstCallPtr& node);

  /**
   * @brief ProcessOpCallNode handles call nodes which are created for any function call
   * ie
   *  Range(...)
   *
   * Meant to handle operators and only extracts the name of the function, then passes to
   * ProcessFunc.
   *
   *
   * @param node
   * @return the op contained by the call ast.
   */
  StatusOr<QLObjectPtr> ProcessOpCallNode(const pypa::AstCallPtr& node);

  /**
   * @brief Processes a subscript call.
   *
   * @param node
   * @return the operator contained by the subscript.
   */
  StatusOr<QLObjectPtr> ProcessSubscriptCall(const pypa::AstSubscriptPtr& node);

  /**
   * @brief Processes an Attribute value, the left side of the attribute data structure.
   * (AstAttribute := <value>.<attribute>; `abc.blah()` -> `abc` is the value of `abc.blah`)
   *
   * @param node attribute node
   * @return  The object of the attribute value or an error if one occurs.
   */
  StatusOr<QLObjectPtr> ProcessAttributeValue(const pypa::AstAttributePtr& node);

  /**
   * @brief Gets the string name of the attribute in an attribute struvct.
   * (AstAttribute := <value>.<attribute>; `abc.blah()` -> `blah` is the attribute of `abc.blah`)
   *
   * @param attr the attribute struct to grab the attribute field from as a string.
   * @return the string representation of the attribute in the attribute structure.
   */
  StatusOr<std::string> GetAttribute(const pypa::AstAttributePtr& attr);

  /**
   * @brief Helper function for processing lists and tuples children into IR nodes.
   *
   * @param elements elements of the input collection
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<std::vector<ExpressionIR*>> the IR representation of the list.
   */
  StatusOr<std::vector<ExpressionIR*>> ProcessCollectionChildren(const pypa::AstExprList& elements,
                                                                 const OperatorContext& op_context);

  /**
   * @brief Processes a list ptr into an IR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<IRNode*> the IR representation of the list.
   */
  StatusOr<ListIR*> ProcessList(const pypa::AstListPtr& ast, const OperatorContext& op_context);

  /**
   * @brief Processes a column subscript ptr into a column IR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<ColumnIR*> the IR representation of the column.
   */
  StatusOr<ColumnIR*> ProcessSubscriptColumn(const pypa::AstSubscriptPtr& ast,
                                             const OperatorContext& op_context);

  /**
   * @brief Processes a tuple ptr into an IR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<IRNode*> the IR representation of the liset.
   */
  StatusOr<TupleIR*> ProcessTuple(const pypa::AstTuplePtr& ast, const OperatorContext& op_context);

  /**
   * @brief Processes a number into an IR Node.
   *
   * @param node
   * @return StatusOr<ExpressionIR*> the IR representation of the number.
   */
  StatusOr<ExpressionIR*> ProcessNumber(const pypa::AstNumberPtr& node);

  /**
   * @brief Processes a str ast ptr into an IR node.
   *
   * @param ast
   * @return StatusOr<ExpressionIR*> the ir representation of the string.
   */
  StatusOr<ExpressionIR*> ProcessStr(const pypa::AstStrPtr& ast);

  /**
   * @brief ProcessData takes in what are typically function arguments and returns the
   * approriate data representation.
   *
   * Ie it might take in an AST tree that represents a list of strings, and convert that into a
   * ListIR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessData(const pypa::AstPtr& ast, const OperatorContext& op_context);

  /**
   * @brief Gets the name string contained within the Name ast node and returns the IRNode
   * referenced by that name, or errors out with an undefined variable.
   *
   * @param name
   * @return StatusOr<OperatorIR*> - The operator referenced by the name, or an error if not
   * found.
   */
  StatusOr<OperatorIR*> LookupName(const pypa::AstNamePtr& name);

  /**
   * @brief Returns the FuncIR::Op struct that corresponds to a python_op
   * representation. This includes the operator code and the full carnot-udf name.
   *
   * @param python_op: the string representation as found in the AST to query on.
   * @param node: the pointer to ast.
   * @return StatusOr<FuncIR::op>: the struct that corresponds to a python op representation.
   */
  StatusOr<FuncIR::Op> GetOp(const std::string& python_op, pypa::AstPtr node);

  /**
   * @brief Handler for Binary operations.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<ExpressionIR*>
   */
  StatusOr<ExpressionIR*> ProcessDataBinOp(const pypa::AstBinOpPtr& node,
                                           const OperatorContext& op_context);

  /**
   * @brief Handler for Bool operations.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<ExpressionIR*>
   */
  StatusOr<ExpressionIR*> ProcessDataBoolOp(const pypa::AstBoolOpPtr& node,
                                            const OperatorContext& op_context);

  /**
   * @brief Processes compare nodes
   *
   * @param node
   * @param op_context
   * @return the evaluated data compare object.
   */
  StatusOr<ExpressionIR*> ProcessDataCompare(const pypa::AstComparePtr& node,
                                             const OperatorContext& op_context);
  /**
   * @brief Handler for functions that are called as args in the data.
   *
   * @param ast
   * @return StatusOr<ExpressionIR*> the ir representation of the data processed by the AST.
   */
  StatusOr<ExpressionIR*> ProcessDataCall(const pypa::AstCallPtr& node,
                                          const OperatorContext& op_context);
  /**
   * @brief  Returns the variable specified by the name pointer.
   * @param node the ast node to run this on.
   * @param name the string to lookup.
   * @return StatusOr<QLObjectPtr>  The pyobject referenced by name, or an error if not found.
   */
  StatusOr<QLObjectPtr> LookupVariable(const pypa::AstPtr& node, const std::string& name);
  StatusOr<QLObjectPtr> LookupVariable(const pypa::AstNamePtr& name) {
    return LookupVariable(name, name->id);
  }
  StatusOr<IRNode*> ProcessDataForAttribute(const pypa::AstAttributePtr& attr);

  StatusOr<ColumnIR*> ProcessSubscriptColumnWithAttribute(const pypa::AstSubscriptPtr& subscript,
                                                          const OperatorContext& op_context);

  static bool IsUnitTimeFn(const std::string& fn_name);
  IR* ir_graph_;
  VarTable var_table_;
  CompilerState* compiler_state_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
