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

#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

using VarTable = std::unordered_map<std::string, IRNode*>;
using LambdaOperatorMap = std::unordered_map<std::string, int64_t>;

#define PYPA_PTR_CAST(TYPE, VAL) \
  std::static_pointer_cast<typename pypa::AstTypeByID<pypa::AstType::TYPE>::Type>(VAL)

#define PYPA_CAST(TYPE, VAL) static_cast<AstTypeByID<AstType::TYPE>::Type&>(VAL)

/**
 * @brief Struct that packages the column names and the expr within the function.
 */
struct LambdaExprReturn {
  LambdaExprReturn() = default;
  explicit LambdaExprReturn(std::string str) : str_(std::move(str)) {}
  LambdaExprReturn(std::string str, bool is_pixie_attr)
      : str_(std::move(str)), is_pixie_attr_(is_pixie_attr) {}
  explicit LambdaExprReturn(ExpressionIR* expr) : expr_(expr) {}
  LambdaExprReturn(ExpressionIR* expr, std::unordered_set<std::string> column_names)
      : input_relation_columns_(std::move(column_names)), expr_(expr) {}
  LambdaExprReturn(ExpressionIR* expr, const LambdaExprReturn& left_expr_ret,
                   const LambdaExprReturn& right_expr_ret)
      : expr_(expr) {
    auto left_set = left_expr_ret.input_relation_columns_;
    auto right_set = right_expr_ret.input_relation_columns_;
    std::set_union(left_set.begin(), left_set.end(), right_set.begin(), right_set.end(),
                   std::inserter(input_relation_columns_, input_relation_columns_.end()));
  }

  /**
   * @brief Returns a merged unordered_set of the columns with this and `ret`s columns.
   *
   * Does manipulate this unordered_set, but assuming that we don't need LambdaExprReturn to stay
   * constant after it's returned
   *
   * @param set of column strings to merge.
   * @return a set that contains this object's columns merged with the arg.
   */
  const std::unordered_set<std::string>& MergeColumns(
      const std::unordered_set<std::string>& input_columns) {
    input_relation_columns_.insert(input_columns.begin(), input_columns.end());
    return input_relation_columns_;
  }
  const std::unordered_set<std::string>& MergeColumns(const LambdaExprReturn& ret) {
    return MergeColumns(ret.input_relation_columns_);
  }

  // The columns we expect to find in the lambda function.
  std::unordered_set<std::string> input_relation_columns_;
  ExpressionIR* expr_ = nullptr;
  std::string str_;
  bool is_pixie_attr_ = false;
  bool StringOnly() const { return expr_ == nullptr && !str_.empty(); }
};

struct LambdaBodyReturn {
  Status AddExpr(const std::string& name, ExpressionIR* expr) {
    col_exprs_.push_back(ColumnExpression{name, expr});
    return Status::OK();
  }
  Status AddColumns(const std::unordered_set<std::string>& new_columns_) {
    input_relation_columns_.insert(new_columns_.begin(), new_columns_.end());
    return Status::OK();
  }
  Status AddExprResult(const std::string& name, const LambdaExprReturn& expr_result) {
    PL_RETURN_IF_ERROR(AddExpr(name, expr_result.expr_));
    PL_RETURN_IF_ERROR(AddColumns(expr_result.input_relation_columns_));
    return Status::OK();
  }
  std::unordered_set<std::string> input_relation_columns_;
  ColExpressionVector col_exprs_;
};

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

class ASTWalker {
 public:
  /**
   * @brief Construct a new ASTWalker object.
   * This constructor will be used at the top level.
   *
   * @param ir_graph
   */
  ASTWalker(std::shared_ptr<IR> ir_graph, CompilerState* compiler_state);

  std::shared_ptr<IR> ir_graph() const { return ir_graph_; }

  /**
   * @brief The entry point into traversal as the root AST is a module.
   *
   * @param node: the ptr to the ast node.
   * @return Status
   */
  Status ProcessModuleNode(const pypa::AstModulePtr& m);

  // Constants for the run-time (UDF) and compile-time fn prefixes.
  inline static constexpr char kRunTimeFuncPrefix[] = "pl";
  inline static constexpr char kCompileTimeFuncPrefix[] = "plc";

  // Constant for the metadata attribute keyword.
  inline static constexpr char kMDKeyword[] = "attr";

  // Constants for operators in the query language.
  inline static constexpr char kFromOpId[] = "From";
  inline static constexpr char kRangeOpId[] = "Range";
  inline static constexpr char kMapOpId[] = "Map";
  inline static constexpr char kBlockingAggOpId[] = "Agg";
  inline static constexpr char kRangeAggOpId[] = "RangeAgg";
  inline static constexpr char kSinkOpId[] = "Result";
  inline static constexpr char kFilterOpId[] = "Filter";
  inline static constexpr char kLimitOpId[] = "Limit";
  inline static constexpr char kJoinOpId[] = "Join";

  // Reserved column names.
  inline static constexpr char kTimeConstantColumnName[] = "time_";

 private:
  /**
   * @brief ProcessArgs traverses an arg_ast tree, confirms that the expected_args are found in that
   * tree, and then returns a map of those expected args to the nodes they point to.
   *
   * @param arg_ast The arglist ast
   * @param op_context: The context of the operator which this is contained within.
   * @param expected_args The string args are expect. Should be ordered if kwargs_only is false.
   * @param default_args A map from the arg name to a defualt node. Every arg is optionally default
   * and doesn't need a specification
   * @return StatusOr<ArgMap> a mapping of arguments (positional and kwargs) to the resulting
   * IRNode.
   */
  StatusOr<ArgMap> ProcessArgs(const pypa::AstCallPtr& call_ast, const OperatorContext& op_context,
                               const std::vector<std::string>& expected_args,
                               const std::unordered_map<std::string, IRNode*>& default_args);

  StatusOr<ArgMap> ProcessArgs(const pypa::AstCallPtr& call_ast, const OperatorContext& op_context,
                               const std::vector<std::string>& expected_args);
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
   * @return StatusOr<OperatorIR*> the op contained by the call ast.
   */
  StatusOr<OperatorIR*> ProcessOpCallNode(const pypa::AstCallPtr& node);

  /**
   * @brief Processes arbitrary operations.
   *
   * @param node
   * @return StatusOr<TOpIR*> The operator that was processed by this function.
   */
  template <typename TOpIR>
  StatusOr<TOpIR*> ProcessOp(const pypa::AstCallPtr& node);

  /**
   * @brief Processes the RangeAgg operator.
   *
   * @param node
   * @return StatusOr<OperatorIR*> the rangeAgg op.
   */
  StatusOr<OperatorIR*> ProcessRangeAggOp(const pypa::AstCallPtr& node);

  /**
   * @brief Processes an Attribute ast at the top level.
   *
   * @param node attribute node that is known to be a function.
   * @return StatusOr<OperatorIR*> The operator represented by the attribute.
   */
  StatusOr<OperatorIR*> ProcessAttribute(const pypa::AstAttributePtr& node);

  /**
   * @brief Processes a list ptr into an IR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<IRNode*> the IR representation of the list.
   */
  StatusOr<IRNode*> ProcessList(const pypa::AstListPtr& ast, const OperatorContext& op_context);

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
   * @return StatusOr<OperatorIR*> - The operator referenced by the name, or an error if not found.
   */
  StatusOr<OperatorIR*> LookupName(const pypa::AstNamePtr& name);

  /**
   * @brief Processes the Lambda args node and returns a mapping of the argument value to respective
   * parent operator to make operator attribution for columns clear.
   *
   * @param node
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<LambdaOperatorMap> A mapping of the lambda argument name to corresponding
   * parent operator.
   */
  StatusOr<LambdaOperatorMap> ProcessLambdaArgs(const pypa::AstLambdaPtr& node,
                                                const OperatorContext& op_context);

  /**
   * @brief Splits apart the Dictionary contained in the lambda fn,
   * evaluates the expression in that dictionary.
   * then returns the Expression map in the Body return.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node : the node that body points to
   * @return StatusOr<LambdaBodyReturn> a struct containing the lambda body.
   */
  StatusOr<LambdaBodyReturn> ProcessLambdaDict(const LambdaOperatorMap& arg_op_map,
                                               const pypa::AstDictPtr& body_dict);

  /**
   * @brief Takes in an attribute contained within a lambda and maps it to either a column or a
   * function call.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaAttribute(const LambdaOperatorMap& arg_op_map,
                                                    const pypa::AstAttributePtr& node);

  /**
   * @brief Helper that assembles functions made within a Lambda.
   * ie the node containing `pl.mean` within
   * >>> lambda r : pl.mean(r.cpu0)
   *
   * @param fn_name
   * @param children_ret_expr
   * @param parent_node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> BuildLambdaFunc(const FuncIR::Op& op, const std::string& prefix,
                                             const std::vector<LambdaExprReturn>& children_ret_expr,
                                             const pypa::AstPtr& parent_node);

  /**
   * @brief Takes a binary operation node and translates it to an IRNode expression.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaBinOp(const LambdaOperatorMap& arg_op_map,
                                                const pypa::AstBinOpPtr& node);
  /**
   * @brief Takes a bool op node and translates it to an IRNode expression.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node
   * @return StatusOr<LambdaExprReturn> bool op contained in the return value.
   */
  StatusOr<LambdaExprReturn> ProcessLambdaBoolOp(const LambdaOperatorMap& arg_op_map,
                                                 const pypa::AstBoolOpPtr& node);

  /**
   * @brief Takes a comparison (<,=,<=,>=,>) node and translates it to an IRNode expression.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaCompare(const LambdaOperatorMap& arg_op_map,
                                                  const pypa::AstComparePtr& node);
  /**
   * @brief Processes a call node with the lambda context (arg_op_map) that helps identify and
   * return the column names we want, and notifies us when there is a column name being used
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator. Used to identify column names.
   * @param node the node we call.
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaCall(const LambdaOperatorMap& arg_op_map,
                                               const pypa::AstCallPtr& node);
  /**
   * @brief Takes in a list and converts it to what's expected in the lambda.
   *
   * Currently restricted to only allow columns in there.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaList(const LambdaOperatorMap& arg_op_map,
                                               const pypa::AstListPtr& node);
  /**
   * @brief Takes an expression and the lambda arg name, processses the expression into an
   * IRNode, and extracts any expected relation values.
   *
   * @param arg_op_map: the string repr of  the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaExpr(const LambdaOperatorMap& arg_op_map,
                                               const pypa::AstPtr& node);

  /**
   * @brief Main entry point for Lambda processing.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<LambdaIR*>
   */
  StatusOr<LambdaIR*> ProcessLambda(const pypa::AstLambdaPtr& ast,
                                    const OperatorContext& op_context);

  /**
   * @brief Make the time now node.
   *
   * @param ast_node
   * @return StatusOr<IntIR*>
   */
  StatusOr<IntIR*> MakeTimeNow(const pypa::AstPtr& ast_node);

  /**
   * @brief Evaluates a now argument as the now time that carnot has.
   *
   * @param arglist The args that are fed tinto this function.
   * @param arglist_parent The parent node used for error reporting.
   * @return StatusOr<IntIR*>
   */
  StatusOr<IntIR*> EvalCompileTimeNow(const pypa::AstArguments& arglist,
                                      const pypa::AstPtr& arglist_parent);

  /**
   * @brief Evaluates the time functions like minutes, seconds, etc.
   *
   * @param attr_fn_name  The time function.
   * @param arglist The args that are fed tinto this function.
   * @param arglist_parent The parent node used for error reporting.
   * @return StatusOr<IntIR*>
   */
  StatusOr<IntIR*> EvalUnitTimeFn(const std::string& attr_fn_name,
                                  const pypa::AstArguments& arglist,
                                  const pypa::AstPtr& arglist_parent);
  /**
   * @brief Evaluates a compile time fn.
   *
   * @param attr_fn_name The fn name.
   * @param arglist The args that are fed tinto this function.
   * @param arglist_parent The parent node used for error reporting.
   * @return StatusOr<IntIR*>
   */
  StatusOr<IntIR*> EvalCompileTimeFn(const std::string& attr_fn_name,
                                     const pypa::AstArguments& arglist,
                                     const pypa::AstPtr& arglist_parent);
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
   * @brief Handler for Binary operations that are run at compile time, not runtime.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<ExpressionIR*>
   */
  StatusOr<ExpressionIR*> ProcessDataBinOp(const pypa::AstBinOpPtr& node,
                                           const OperatorContext& op_context);
  /**
   * @brief Handler for functions that are called as args in the data.
   * Calls nested within Lambda trees are not touched by this.
   *
   * @param ast
   * @return StatusOr<ExpressionIR*> the ir representation of the data processed by the AST.
   */
  StatusOr<ExpressionIR*> ProcessDataCall(const pypa::AstCallPtr& node);

  /**
   * @brief Processes nested attributes in the lambda function. For now this is just metadata
   * references.
   *
   * @param arg_op_map: the string repr of the lambda argument that is currently being traversed.
   * Used to connect Columns as references to the parent operator.
   * @param attribute_value: the string representation of the attribute that calls on the
   * preant_attribute
   * @param parent_attr: the parent attribute that the original attribute is called upon.
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaNestedAttribute(const LambdaOperatorMap& arg_op_map,
                                                          const std::string& attribute_str,
                                                          const pypa::AstAttributePtr& parent_attr);

  /**
   * @brief Processes functions that are argless.
   *
   * @param function_prefix: the prefix of the calling function.
   * @param function_name: the string representation of the function.
   * @return StatusOr<LambdaExprReturn> container of the function expression.
   */
  StatusOr<LambdaExprReturn> ProcessArglessFunction(const std::string& function_prefix,
                                                    const std::string& function_name);

  /**
   * @brief Creates and returns an IR representation of a column given a column name.
   *
   * @param column_name: the column name string.
   * @param column_ast_node: the referring ast_node of the column.
   * @param parent_op_idx: the index of the parent of the operator that this column refers to.
   * @return StatusOr<LambdaExprReturn>: container of the expression.
   */
  StatusOr<LambdaExprReturn> ProcessRecordColumn(const std::string& column_name,
                                                 const pypa::AstPtr& column_ast_node,
                                                 int64_t parent_op_idx);
  /**
   * @brief Processes a metadata attribute.
   *
   * @param arg_op_map: the argument of the containing lambda.
   * @param attribute_value: the value of the attribute.
   * @param val_attr: the containing attribute ptr fo the
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessMetadataAttribute(const LambdaOperatorMap& arg_op_map,
                                                      const std::string& attribute_value,
                                                      const pypa::AstAttributePtr& val_attr);
  /**
   * @brief Create an error that incorporates line, column of ast node into the error message.
   *
   * @param err_msg
   * @param ast
   * @return Status
   */
  template <typename... Args>
  static Status CreateAstError(const pypa::AstPtr& ast, Args... args);
  template <typename... Args>
  static Status CreateAstError(const pypa::Ast& ast, Args... args);

  /**
   * @brief Returns the string repr of an Ast Type.
   * @param The AstType type.
   * @return std::string representation of the type.
   */
  static std::string GetAstTypeName(pypa::AstType type);

  /**
   * @brief Get the Id from the NameAST.
   *
   * @param node
   * @return std::string
   */
  static const std::string GetNameID(const pypa::AstPtr& node) {
    return PYPA_PTR_CAST(Name, node)->id;
  }

  /**
   * @brief Gets the string out of what is suspected to be a strAst. Errors out if ast is not of
   * type str.
   *
   * @param ast
   * @return StatusOr<std::string>
   */
  static StatusOr<std::string> GetStrAstValue(const pypa::AstPtr& ast) {
    if (ast->type != pypa::AstType::Str) {
      return CreateAstError(ast, "Expected string type. Got $0", GetAstTypeName(ast->type));
    }
    return PYPA_PTR_CAST(Str, ast)->value;
  }
  static bool IsUnitTimeFn(const std::string& fn_name);
  std::shared_ptr<IR> ir_graph_;
  VarTable var_table_;
  CompilerState* compiler_state_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
