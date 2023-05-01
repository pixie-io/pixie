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

#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/planner/ast/ast_visitor.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/var_table.h"
#include "src/carnot/planner/plannerpb/service.pb.h"
#include "src/carnot/planner/probes/probes.h"
#include "src/carnot/planner/probes/tracing_module.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using FuncToExecute = plannerpb::FuncToExecute;
using ExecFuncs = std::vector<FuncToExecute>;
using ArgValues = std::vector<FuncToExecute::ArgValue>;
using ModuleHandler = absl::flat_hash_map<std::string, QLObjectPtr>;

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
  /**
   * @brief Creates a top-level AST Visitor with the given graph and compiler state.
   * Variable table and the pixie module are created with this call.
   *
   * @param graph
   * @param var_table
   * @param module_handler
   * @param compiler_state
   * @param module_handler
   * @param reserved_names
   * @param module_map
   * @return StatusOr<std::shared_ptr<ASTVisitorImpl>>
   */
  static StatusOr<std::shared_ptr<ASTVisitorImpl>> Create(
      IR* graph, MutationsIR* mutations, CompilerState* compiler_state,
      ModuleHandler* module_handler, bool func_based_exec = false,
      const absl::flat_hash_set<std::string>& reserved_names = {},
      const absl::flat_hash_map<std::string, std::string>& module_map = {});

  /**
   * @brief Creates a top-level AST Visitor.
   *
   *
   * @param graph
   * @param global_var_table
   * @param var_table
   * @param module_handler
   * @param compiler_state
   * @param module_handler
   * @param reserved_names
   * @param module_map
   * @return StatusOr<std::shared_ptr<ASTVisitorImpl>>
   */
  static StatusOr<std::shared_ptr<ASTVisitorImpl>> Create(
      IR* graph, std::shared_ptr<VarTable> var_table, MutationsIR* mutations,
      CompilerState* compiler_state, ModuleHandler* module_handler, bool func_based_exec = false,
      const absl::flat_hash_set<std::string>& reserved_names = {},
      const absl::flat_hash_map<std::string, std::string>& module_map = {});

  /**
   * @brief Creates a child of this visitor, sharing the graph,
   * compiler_state, and creates a vartable that is a child of this visitor's var_table.
   *
   * @return std::shared_ptr<ASTVisitorImpl>
   */
  std::shared_ptr<ASTVisitorImpl> CreateChild();

  /**
   * @brief Creates a child of this visitor, sharing the graph,
   * compiler_state, but takes in an outside var_table. Used to parse modules.
   *
   * @return std::shared_ptr<ASTVisitor> The Child Module Visitor.
   */
  std::shared_ptr<ASTVisitor> CreateModuleVisitor(std::shared_ptr<VarTable> var_table) override;

  Status AddPixieModule() { return AddPixieModule(PixieModule::kPixieModuleObjName); }

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
   * @param m the pointer to the parsed ast module.
   * @param op_context the operator context to operate with.
   * @return StatusOr<QLObjectPtr> the QL object representation of the expression, or an error if
   * the operator fails.
   */
  StatusOr<QLObjectPtr> ProcessSingleExpressionModule(const pypa::AstModulePtr& m) override;

  /**
   * @brief Parses and processes out a single expression in the form of an IRNode.
   *
   * @param str the input string
   * @param import_px whether or not to import the pixie module before parsing the expression.
   * @param ParseAndProcessSingleExpression
   * @return StatusOr<QLObjectPtr> the QL object of the expression or an error if something fails
   * during processing.
   */
  StatusOr<QLObjectPtr> ParseAndProcessSingleExpression(std::string_view str,
                                                        bool import_px) override;

  /**
   * @brief Process functions that are to be executed.
   *
   * @param exec_funcs list of specifications for functions to call.
   *
   * @return Status
   */
  Status ProcessExecFuncs(const ExecFuncs& exec_funcs) override;

  IR* ir_graph() const { return ir_graph_; }
  std::shared_ptr<VarTable> var_table() const { return var_table_; }

  // Reserved keywords
  inline static constexpr char kTimeConstantColumnName[] = "time_";
  inline static constexpr char kStringTypeName[] = "str";
  inline static constexpr char kIntTypeName[] = "int";
  inline static constexpr char kFloatTypeName[] = "float";
  inline static constexpr char kBoolTypeName[] = "bool";
  inline static constexpr char kListTypeName[] = "list";
  inline static constexpr char kNoneName[] = "None";
  inline static constexpr char kTrueName[] = "True";
  inline static constexpr char kFalseName[] = "False";

 private:
  /**
   * @brief Construct a new ASTWalker object.
   * This constructor will be used at the top level.
   *
   * @param ir_graph
   */
  ASTVisitorImpl(IR* ir_graph, MutationsIR* mutations, CompilerState* compiler_state,
                 std::shared_ptr<VarTable> global_var_table, std::shared_ptr<VarTable> var_table,
                 bool func_based_exec, const absl::flat_hash_set<std::string>& reserved_names,
                 ModuleHandler* module_handler, const std::shared_ptr<udf::Registry>& udf_registry)
      : ir_graph_(ir_graph),
        compiler_state_(compiler_state),
        global_var_table_(global_var_table),
        var_table_(var_table),
        func_based_exec_(func_based_exec),
        reserved_names_(reserved_names),
        module_handler_(module_handler),
        mutations_(mutations),
        udf_registry_(udf_registry) {}

  Status InitGlobals();
  Status CreateBoolLiterals();

  /**
   * @brief Process statements list passed in as an argument.
   *
   * @param body the body ast object to parse.
   * @param is_function_definition_body whether the body is in a function definition. There are some
   * extra steps that occur when is_function_definition_body is true, ie processing Return
   * statements.
   * @return Status any errors that come up.
   */
  StatusOr<QLObjectPtr> ProcessASTSuite(const pypa::AstSuitePtr& body,
                                        bool is_function_definition_body);

  /**
   * @brief Processes any expression node into a QLObjectPtr.
   *
   * @param node
   * @return StatusOr<QLObjectPtr>
   */
  StatusOr<QLObjectPtr> Process(const pypa::AstExpr& node, const OperatorContext& op_context);

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
   * @brief ProcessExprStmtNode handles full lines that are expression statements.
   * ie in the following lines
   *  1: a = px.DataFrame(...)
   *  2: a.drop(...)
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
   * @brief ProcessSubscriptAssignment (& Attribute) handles lines with statements like
   *  1: a['foo'] = a['bar'] * 2 + a.abc
   *  1: a.foo = a.bar * 2 + a['col with space']
   *   *
   * @param assign_node the target of the assignment
   * @param expr_node the expression of the assignment
   * @return Status whether the assignment worked or not.
   */
  Status ProcessSubscriptAssignment(const pypa::AstSubscriptPtr& assign_node,
                                    const pypa::AstExpr& expr_node);
  Status ProcessAttributeAssignment(const pypa::AstAttributePtr& assign_node,
                                    const pypa::AstExpr& expr_node);
  Status ProcessMapAssignment(const pypa::AstPtr& assign_target,
                              std::shared_ptr<Dataframe> parent_df, QLObjectPtr value_obj,
                              const pypa::AstExpr& expr_node);
  /**
   * @brief ProcessAssignNode handles lines where an expression is assigned to a value.
   * ie in the following lines
   *  1: a = px.DataFrame(...)
   *  2: a.drop(...)
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
   * @brief ProcessFunctionDefNode handles function definitions in the query language.
   * ```
   * def func(blah):
   *     return blah
   * ```
   *
   * @param node
   * @return Status of whether creation the function definition worked or not.
   */
  Status ProcessFunctionDefNode(const pypa::AstFunctionDefPtr& node);

  /**
   * @brief ProcessImport handles imports definitions in the query language.
   * Only supports px and pxtrace right now.
   * ```
   * import px
   * ```
   *
   * @param node
   * @return Status of whether creation the import worked or not.
   */
  Status ProcessImport(const pypa::AstImportPtr& node);

  /**
   * @brief ProcessImportFrom handles from imports definitions in the query language.
   * Only supports px and pxtrace right now.
   * ```
   * from px import now
   * ```
   *
   * @param node
   * @return Status of whether creation the import worked or not.
   */
  Status ProcessImportFrom(const pypa::AstImportFromPtr& node);

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
   *  drop(...)
   *
   * Meant to handle operators and only extracts the name of the function, then passes to
   * ProcessFunc.
   *
   *
   * @param node
   * @return the op contained by the call ast.
   */
  StatusOr<QLObjectPtr> ProcessCallNode(const pypa::AstCallPtr& node,
                                        const OperatorContext& op_context);

  /**
   * @brief Processes a subscript call.
   *
   * @param node
   * @return the operator contained by the subscript.
   */
  StatusOr<QLObjectPtr> ProcessSubscriptCall(const pypa::AstSubscriptPtr& node,
                                             const OperatorContext& op_context);

  /**
   * @brief Validates whether the value you subscript on is valid. Only does this if on the
   * MapContext.
   *
   * @param node
   * @param op_context
   * @return StatusOr<QLObjectPtr>
   */
  Status ValidateSubscriptValue(const pypa::AstExpr& node, const OperatorContext& op_context);

  /**
   * @brief Processes an Attribute and returns the QLObject that the attribute references.
   *
   * @param node
   * @return StatusOr<QLObjectPtr>
   */
  StatusOr<QLObjectPtr> ProcessAttribute(const pypa::AstAttributePtr& node,
                                         const OperatorContext& op_context);

  /**
   * @brief Gets the string name of the attribute in an attribute struct.
   * (AstAttribute := <value>.<attribute>; `abc.blah()` -> `blah` is the attribute of `abc.blah`)
   *
   * @param attr the attribute struct to grab the attribute field from as a string.
   * @return the string representation of the attribute in the attribute structure.
   */
  StatusOr<std::string> GetAttributeStr(const pypa::AstAttributePtr& attr);

  /**
   * @brief Helper function for processing lists and tuples children into IR nodes.
   *
   * @param elements elements of the input collection
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<std::vector<QLObjectPtr>> the QLObject representation of the list.
   */
  StatusOr<std::vector<QLObjectPtr>> ProcessCollectionChildren(const pypa::AstExprList& elements,
                                                               const OperatorContext& op_context);

  /**
   * @brief Processes a list ptr into an IR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<IRNode*> the IR representation of the list.
   */
  StatusOr<QLObjectPtr> ProcessList(const pypa::AstListPtr& ast, const OperatorContext& op_context);

  /**
   * @brief Processes a tuple ptr into an IR node.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<IRNode*> the IR representation of the tuple.
   */
  StatusOr<QLObjectPtr> ProcessTuple(const pypa::AstTuplePtr& ast,
                                     const OperatorContext& op_context);

  /**
   * @brief Processes a number into an IR Node.
   *
   * @param node
   * @return StatusOr<ExpressionIR*> the IR representation of the number.
   */
  StatusOr<QLObjectPtr> ProcessNumber(const pypa::AstNumberPtr& node);

  /**
   * @brief Processes a str ast ptr into an IR node.
   *
   * @param ast
   * @return StatusOr<ExpressionIR*> the ir representation of the string.
   */
  StatusOr<QLObjectPtr> ProcessStr(const pypa::AstStrPtr& ast);

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
   * @brief Returns the FuncIR::Op struct that corresponds to a python_op representation for a unary
   * operator.
   *
   * @param python_op: the string representation as found in the AST.
   * @param node: the pointer to ast.
   * @return StatusOr<FuncIR::Op>: the struct that corresponds to a python op representation.
   */
  StatusOr<FuncIR::Op> GetUnaryOp(const std::string& python_op, const pypa::AstPtr node);
  /**
   * @brief Handler for Binary operations.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<ExpressionIR*>
   */
  StatusOr<QLObjectPtr> ProcessDataBinOp(const pypa::AstBinOpPtr& node,
                                         const OperatorContext& op_context);

  /**
   * @brief Handler for Bool operations.
   *
   * @param ast
   * @param op_context: The context of the operator which this is contained within.
   * @return StatusOr<ExpressionIR*>
   */
  StatusOr<QLObjectPtr> ProcessDataBoolOp(const pypa::AstBoolOpPtr& node,
                                          const OperatorContext& op_context);

  /**
   * @brief Processes compare nodes
   *
   * @param node
   * @param op_context
   * @return the evaluated data compare object.
   */
  StatusOr<QLObjectPtr> ProcessDataCompare(const pypa::AstComparePtr& node,
                                           const OperatorContext& op_context);
  /**
   * @brief Processes unary operator nodes
   *
   * @param node
   * @param op_context
   * @return the evaluated unary operator object
   */
  StatusOr<QLObjectPtr> ProcessDataUnaryOp(const pypa::AstUnaryOpPtr& node,
                                           const OperatorContext& op_context);
  /**
   * @brief Processes a Dict AST object into a DictObject.
   *
   * @param node
   * @param op_context
   * @return StatusOr<QLObjectPtr> the dict object or error that occurred while processing the
   * object.
   */
  StatusOr<QLObjectPtr> ProcessDict(const pypa::AstDictPtr& node,
                                    const OperatorContext& op_context);
  /**
   *
   * @brief  Returns the variable specified by the name pointer.
   * @param node the ast node to run this on.
   * @param name the string to lookup.
   * @return StatusOr<QLObjectPtr>  The pyobject referenced by name, or an error if not found.
   */
  StatusOr<QLObjectPtr> LookupVariable(const pypa::AstPtr& node, const std::string& name);
  StatusOr<QLObjectPtr> LookupVariable(const pypa::AstNamePtr& name) {
    return LookupVariable(name, name->id);
  }

  /**
   * @brief This is the FuncObject caller logic for functions defined inside of the query
   *
   * @param arg_names The name of the arguments that are passed in.
   * @param body the body of the defined function
   * @param ast the ast node of the function call
   * @param args the parsed arguments passed into the function during a call.
   * @return StatusOr<QLObjectPtr>
   */
  StatusOr<QLObjectPtr> FuncDefHandler(
      const std::vector<std::string>& arg_names,
      const absl::flat_hash_map<std::string, QLObjectPtr>& arg_annotation_objs,
      const pypa::AstSuitePtr& body, const pypa::AstPtr& ast, const ParsedArgs& args);

  /**
   * @brief Returns an error if the arg does not match the annotation.
   *
   * @param arg
   * @param annotation
   * @return Status
   */
  Status DoesArgMatchAnnotation(QLObjectPtr arg, QLObjectPtr annotation_obj);

  /**
   * @brief Handles the return statements of function definitions.
   *
   * @param ret the return ast node
   * @return StatusOr<QLObjectPtr> the ql object ptr meant by the return statement.
   */
  StatusOr<QLObjectPtr> ProcessFuncDefReturn(const pypa::AstReturnPtr& ret);

  Status AddPixieModule(std::string_view module_name);
  /**
   * @brief Processes any doc string within a function.
   *
   * @param body the ast node of the body of the function
   * @return StatusOr<QLObjectPtr> the ql object ptr representing the doc string (either a string or
   * None)
   */
  StatusOr<QLObjectPtr> ProcessFuncDefDocString(const pypa::AstSuitePtr& body);

  /**
   * @brief Processes a doc string pypa node.
   *
   * @param doc_string the ast node of the doc string.
   * @return StatusOr<QLObjectPtr> the ql object ptr representing the doc string.
   */
  StatusOr<QLObjectPtr> ProcessDocString(const pypa::AstDocStringPtr& doc_string);

  /**
   * @brief Processes FuncToExecute_ArgValues into an ArgMap
   *
   * @param func the function to check types against.
   * @param arg_values the list of FuncToExecute_ArgValues to process.
   * @return StatusOr<ArgMap> the arg map after processing.
   */
  StatusOr<ArgMap> ProcessExecFuncArgs(const pypa::AstPtr& ast,
                                       const std::shared_ptr<FuncObject>& func,
                                       const ArgValues& arg_values);

  /**
   * @brief Parses a string as the given type. In the future, we will parse the string as an
   * expression.
   *
   * @param ast ast pointer for error context.
   * @param value string value to parse.
   * @param type TypeObject ptr specifying the expected type.
   * @return StatusOr<QLObjectPtr> the parsed expression object.
   */
  StatusOr<QLObjectPtr> ParseStringAsType(const pypa::AstPtr& ast, const std::string& value,
                                          const std::shared_ptr<TypeObject>& type);

  Status SetupModules(const absl::flat_hash_map<std::string, std::string>& module_name_to_pxl_map);

  /**
   * @brief Creates a child of this visitor, sharing the graph,
   * compiler_state, and creates a vartable that is a child of this visitor's var_table.
   * Internal
   *
   * @return std::shared_ptr<ASTVisitorImpl>
   */
  std::shared_ptr<ASTVisitorImpl> CreateChildImpl(std::shared_ptr<VarTable> var_table);

  IR* ir_graph_;
  CompilerState* compiler_state_;
  // The global variable table available to all visitors.
  std::shared_ptr<VarTable> global_var_table_;
  std::shared_ptr<VarTable> var_table_;
  bool func_based_exec_;
  absl::flat_hash_set<std::string> reserved_names_;
  // The object that holds onto modules. Added separately from VarTable to prevent re-compilation of
  // modules.
  ModuleHandler* module_handler_;
  // The IR holding mutation information.
  MutationsIR* mutations_;
  // Compile time registry for udfs. Used to execute constant expressions which simplifies
  // expression management for arguments.
  std::shared_ptr<udf::Registry> udf_registry_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
