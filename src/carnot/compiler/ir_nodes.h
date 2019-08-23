#pragma once
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>

#include "src/carnot/compiler/compiler_error_context.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/metadatapb/metadata.pb.h"
#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace compiler {

class IR;
class IRNode;
using IRNodePtr = std::unique_ptr<IRNode>;
using ArgMap = std::unordered_map<std::string, IRNode*>;
using table_store::schema::Relation;

enum class IRNodeType {
  kAny = -1,
  kMemorySource,
  kMemorySink,
  kRange,
  kMap,
  kBlockingAgg,
  kFilter,
  kLimit,
  kString,
  kFloat,
  kInt,
  kBool,
  kFunc,
  kList,
  kLambda,
  kColumn,
  kTime,
  kMetadata,
  kMetadataResolver,
  kMetadataLiteral,
  kGRPCSourceGroup,
  kGRPCSource,
  kGRPCSink,
  kUnion,
  kJoin,
  number_of_types  // This is not a real type, but is used to verify strings are inline
                   // with enums.
};
static constexpr const char* kIRNodeStrings[] = {"MemorySource",
                                                 "MemorySink",
                                                 "Range",
                                                 "Map",
                                                 "BlockingAgg",
                                                 "Filter",
                                                 "Limit",
                                                 "String",
                                                 "Float",
                                                 "Int",
                                                 "Bool",
                                                 "Func",
                                                 "List",
                                                 "Lambda",
                                                 "Column",
                                                 "Time",
                                                 "Metadata",
                                                 "MetadataResolver",
                                                 "MetadataLiteral",
                                                 "GRPCSourceGroup",
                                                 "GRPCSource",
                                                 "GRPCSink",
                                                 "Union",
                                                 "Join"};

/**
 * @brief Node class for the IR.
 *
 * Each Operator that overlaps IR and LogicalPlan can notify the compiler by returning true in the
 * overloaded HasLogicalRepr method.
 */
class IRNode {
 public:
  IRNode() = delete;
  virtual ~IRNode() = default;
  /**
   * @return whether or not the node has a logical representation.
   */
  virtual bool HasLogicalRepr() const = 0;
  int64_t line() const { return line_; }
  int64_t col() const { return col_; }
  bool line_col_set() const { return line_col_set_; }

  virtual std::string DebugString() const;
  virtual bool IsOperator() const = 0;
  virtual bool IsExpression() const = 0;
  bool is_source() const { return is_source_; }
  IRNodeType type() const { return type_; }
  std::string type_string() const { return kIRNodeStrings[static_cast<int64_t>(type())]; }
  /**
   * @brief Set the pointer to the graph.
   * The pointer is passed in by the Node factory of the graph
   * (see IR::MakeNode) so that we can add edges between this
   * object and any other objects created later on.
   *
   * @param graph_ptr : pointer to the graph object.
   */
  void SetGraphPtr(IR* graph_ptr) { graph_ptr_ = graph_ptr; }
  // Returns the ID of the operator.
  int64_t id() const { return id_; }
  IR* graph_ptr() const { return graph_ptr_; }
  pypa::AstPtr ast_node() const { return ast_node_; }
  /**
   * @brief Create an error that incorporates line, column of ir node into the error message.
   *
   * @param args: the arguments to the substitute that is called.
   * @return Status: Status with CompilerError context.
   */
  template <typename... Args>
  Status CreateIRNodeError(Args... args) const {
    compilerpb::CompilerErrorGroup context =
        LineColErrorPb(line(), col(), absl::Substitute(args...));
    return Status(statuspb::INVALID_ARGUMENT, "",
                  std::make_unique<compilerpb::CompilerErrorGroup>(context));
  }
  /**
   * @brief Errors out if in debug mode, otherwise floats up an error.
   *
   * @return Status: status if not in debug mode.
   */
  template <typename... Args>
  Status DExitOrIRNodeError(Args... args) const {
    DCHECK(false) << absl::Substitute(args...);
    return CreateIRNodeError(args...);
  }

  /*
   * @brief DeepClone this node into a new graph. All children classes need to implement
   * DeepCloneIntoImpl. If a child class is itself a parent of other classes, then it must override
   * this class and call this method, followed by whatever operations that all of it's child
   * classes must do during a DeepClone.
   *
   * @param graph
   * @return StatusOr<IRNode*>
   */
  virtual StatusOr<IRNode*> DeepCloneInto(IR* graph) const;

 protected:
  explicit IRNode(int64_t id, IRNodeType type, bool is_source)
      : id_(id), type_(type), is_source_(is_source) {}
  void SetLineCol(int64_t line, int64_t col);
  void SetLineCol(const pypa::AstPtr& ast_node);
  /**
   * @brief The implementation of DeepCloneInto to be overridden by children of this class.
   *
   * @param graph
   * @return StatusOr<IRNode*>
   */
  virtual StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const = 0;

 private:
  int64_t id_;
  // line and column where the parser read the data for this node.
  // used for highlighting errors in queries.
  int64_t line_;
  int64_t col_;
  IR* graph_ptr_;
  IRNodeType type_;
  bool line_col_set_ = false;
  bool is_source_ = false;
  pypa::AstPtr ast_node_;
};

class OperatorIR;
/**
 * IR contains the intermediate representation of the query
 * before compiling into the logical plan.
 */
class IR {
 public:
  /**
   * @brief Node factory that adds a node to the list,
   * updates an id, then returns a pointer to manipulate.
   *
   * The object will be owned by the IR object that created it.
   *
   * @tparam TOperator the type of the operator.
   * @return StatusOr<TOperator *> - the node will be owned
   * by this IR object.
   */
  template <typename TOperator>
  StatusOr<TOperator*> MakeNode() {
    return MakeNode<TOperator>(id_node_counter);
  }
  template <typename TOperator>
  StatusOr<TOperator*> MakeNode(int64_t id) {
    id_node_counter = std::max(id + 1, id_node_counter);
    auto node = std::make_unique<TOperator>(id);
    dag_.AddNode(node->id());
    node->SetGraphPtr(this);
    TOperator* raw = node.get();
    id_node_map_.emplace(node->id(), std::move(node));
    return raw;
  }

  Status AddEdge(int64_t from_node, int64_t to_node);
  bool HasNode(int64_t node_id) { return dag().HasNode(node_id); }

  Status AddEdge(IRNode* from_node, IRNode* to_node);
  Status DeleteEdge(int64_t from_node, int64_t to_node);
  Status DeleteNode(int64_t node);
  Status DeleteNodeAndChildren(int64_t node);
  plan::DAG& dag() { return dag_; }
  const plan::DAG& dag() const { return dag_; }
  std::string DebugString();
  IRNode* Get(int64_t id) const {
    DCHECK(dag_.HasNode(id)) << "DAG doesn't have node: " << id;
    auto iterator = id_node_map_.find(id);
    DCHECK(iterator != id_node_map_.end()) << "id to node map doesn't contain id: " << id;
    return iterator->second.get();
  }
  size_t size() const { return id_node_map_.size(); }
  StatusOr<std::vector<IRNode*>> GetSinks() {
    std::vector<IRNode*> nodes;
    for (auto& i : dag().TopologicalSort()) {
      IRNode* node = Get(i);
      if (node->type() == IRNodeType::kMemorySink) {
        nodes.push_back(node);
        DCHECK(node->IsOperator());
      }
    }
    if (nodes.empty()) {
      return error::InvalidArgument("No Result() found in the graph.");
    }
    return nodes;
  }
  StatusOr<std::unique_ptr<IR>> Clone() const;

  StatusOr<planpb::Plan> ToProto() const;

  /**
   * @brief Removes the nodes and edges listed in the following set.
   *
   * @param ids_to_prune: the ids which to prune from the graph.
   * @return Status: error if something not found or missing.
   */
  Status Prune(const std::unordered_set<int64_t>& ids_to_prune);

 private:
  Status OutputProto(planpb::PlanFragment* pf, const OperatorIR* op_node) const;
  plan::DAG dag_;
  std::unordered_map<int64_t, IRNodePtr> id_node_map_;
  int64_t id_node_counter = 0;
};

class ColumnIR;

/**
 * @brief Node class for the operator
 *
 */
class OperatorIR : public IRNode {
 public:
  OperatorIR() = delete;
  bool IsOperator() const override { return true; }
  bool IsExpression() const override { return false; }
  table_store::schema::Relation relation() const { return relation_; }
  Status SetRelation(table_store::schema::Relation relation) {
    relation_init_ = true;
    relation_ = relation;
    return Status::OK();
  }
  bool IsRelationInit() const { return relation_init_; }
  bool HasParents() const { return parents_.size() != 0; }
  bool IsChildOf(OperatorIR* parent) {
    return std::find(parents_.begin(), parents_.end(), parent) != parents_.end();
  }
  const std::vector<OperatorIR*>& parents() const { return parents_; }
  Status AddParent(OperatorIR* node);
  Status RemoveParent(OperatorIR* op);

  /**
   * @brief Replaces the old_parent with the new parent. Errors out if the old_parent isn't actually
   * a parent.
   *
   * @param old_parent: operator's parent that will be replaced.
   * @param new_parent: the new operator to replace old_parent with.
   * @return Status: Error if old_parent not an actualy parent.
   */
  Status ReplaceParent(OperatorIR* old_parent, OperatorIR* new_parent);

  /**
   * @brief Initializes the Operator with a single parent.
   *
   * @param parent: parent operator, or a nullptr if no parent for this operator.
   * @param args: the map of string to IRNode that represents
   * @param ast_node: the node in the ast parser to initialize this.
   * @return Status: error if anything fails during this call.
   */
  Status Init(OperatorIR* parent, const ArgMap& args, const pypa::AstPtr& ast_node);

  /**
   * @brief Initializes the Operator with multiple parents.
   *
   * @param parents: parent operators, or an empty vector.
   * @param args: the map of string to IRNode that represents
   * @param ast_node: the node in the ast parser to initialize this.
   * @return Status: error if anything fails during this call.
   */
  Status Init(std::vector<OperatorIR*> parents, const ArgMap& args, const pypa::AstPtr& ast_node);
  virtual Status InitImpl(const ArgMap& args) = 0;
  virtual std::vector<std::string> ArgKeys() = 0;

  /**
   * @brief Returns the default argument values for any argument passed in.
   * Does not need to be overridden in an Operator definition if an Operator has no default
   * arguments.
   *
   * TODO(philkuz)(PL_804) make the mapping to an Lambda that takes in a graph_ptr and returns a new
   * IRNode instead of doing this.
   * @return Stirng to IR mapping
   */
  virtual std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) {
    return std::unordered_map<std::string, IRNode*>();
  }
  virtual Status ToProto(planpb::Operator*) const = 0;
  // Checks whether the passed in arg maps contains the expected keys in this init function.
  Status ArgMapContainsKeys(const ArgMap& args);

  Status EvaluateExpression(planpb::ScalarExpression* expr, const IRNode& ir_node) const;

  std::string ParentsDebugString();
  Status CopyParents(OperatorIR* og_op) const;

  /**
   * @brief Override of DeepCloneInto that adds special handling for Operators.
   *
   * @param graph: the graph which to clone into. Should not be the same graph.
   * @return StatusOr<IRNode*>: The copied node into the new graph.
   */
  StatusOr<IRNode*> DeepCloneInto(IR* graph) const override;

  virtual bool IsBlocking() const { return false; }

  /**
   * @brief Returns the Operator children of this node.
   *
   * @return std::vector<OperatorIR*>: the vector of operator children of this node.
   */
  std::vector<OperatorIR*> Children() const;

 protected:
  explicit OperatorIR(int64_t id, IRNodeType type, bool has_parents, bool is_source)
      : IRNode(id, type, is_source), can_have_parents_(has_parents) {}

 private:
  table_store::schema::Relation relation_;
  bool relation_init_ = false;
  bool can_have_parents_;
  std::vector<OperatorIR*> parents_;
};

class ExpressionIR : public IRNode {
 public:
  ExpressionIR() = delete;

  bool IsOperator() const override { return false; }
  bool IsExpression() const override { return true; }
  virtual types::DataType EvaluatedDataType() const = 0;
  virtual bool IsDataTypeEvaluated() const = 0;
  virtual bool IsColumn() const { return false; }
  virtual bool IsData() const { return false; }
  StatusOr<OperatorIR*> ContainingOperator() const {
    IR* graph = graph_ptr();
    int64_t cur_id = id();
    while (!graph->Get(cur_id)->IsOperator()) {
      std::vector<int64_t> parents = graph->dag().ParentsOf(cur_id);
      if (parents.size() > 1) {
        std::vector<std::string> parent_strs;
        for (const int64_t& p : parents) {
          IRNode* parent = graph->Get(p);
          parent_strs.push_back(absl::Substitute("$0(id=$1)", parent->type_string(), p));
        }
        return CreateIRNodeError(
            "Found more than one parent for node(id=$0,type=$1) while searching for parent "
            "operator. Parents:[$2]",
            cur_id, graph->Get(cur_id)->type_string(), absl::StrJoin(parent_strs, ","));
      }
      if (parents.size() == 0) {
        return CreateIRNodeError(
            "Got no parents for node(id=$0,type=$1) while searching for parent operator. ", cur_id,
            graph->Get(cur_id)->type_string());
      }
      cur_id = parents[0];
    }
    return static_cast<OperatorIR*>(graph->Get(cur_id));
  }

 protected:
  ExpressionIR(int64_t id, IRNodeType type) : IRNode(id, type, false) {}
};

class MetadataProperty : public NotCopyable {
 public:
  MetadataProperty() = delete;
  virtual ~MetadataProperty() = default;

  /**
    @brief Returns a bool that notifies whether an expression fits the expected format for this
   * property when comparing.  This is used to make sure comparison operations (==, >, <, !=) are
   * pre-checked during compilation, preventing unnecssary operations during execution and exposing
   * query errors to the user.
   *
   * For example, we expect values compared to POD_NAMES to be Strings of the format
   * `<namespace>/<pod-name>`.
   *
   * ExplainFormat should describe the expected format of this method in string form.
   *
   * @param value: the ExpressionIR node that
   */
  virtual bool ExprFitsFormat(ExpressionIR* value) const = 0;

  /**
   * @brief Describes the Expression format that ExprFitsFormat expects.
   * This will be passed up to query writing users so you should prioritize pythonic descriptions
   * over internal compiler jargon.
   */
  virtual std::string ExplainFormat() const = 0;

  /**
   * @brief Returns the Carnot column-name as a string.
   */
  inline std::string GetColumnRepr() const { return FormatMetadataColumn(name_); }

  /**
   * @brief Returns the key columns formatted as metadata columns.
   */
  std::vector<std::string> GetKeyColumnReprs() const {
    std::vector<std::string> columns;
    for (const auto& c : key_columns_) {
      columns.push_back(FormatMetadataColumn(c));
    }
    return columns;
  }

  /**
   * @brief Returns whether this metadata key can be obtained by the provided key column.
   * @param key: the column name string as seen in Carnot.
   */
  inline bool HasKeyColumn(const std::string_view key) {
    auto columns = GetKeyColumnReprs();
    return std::find(columns.begin(), columns.end(), key) != columns.end();
  }

  /**
   * @brief Returns the udf-string that converts a given key column to the Metadata
   * represented by this property.
   */
  StatusOr<std::string> UDFName(const std::string_view key) {
    if (!HasKeyColumn(key)) {
      return error::InvalidArgument(
          "Key column $0 invalid for metadata value $1. Expected one of [$2].", key, name_,
          absl::StrJoin(key_columns_, ","));
    }
    return absl::Substitute("$1_to_$0", name_, ExtractMetadataFromColumnName(key));
  }

  // Getters.
  inline std::string name() const { return name_; }
  inline metadatapb::MetadataType metadata_type() const { return metadata_type_; }
  inline types::DataType column_type() const { return column_type_; }

  /**
   * @brief Return a string that adds the Metadata column prefix to the passed in argument.
   */
  inline static std::string FormatMetadataColumn(const std::string_view col_name) {
    return absl::Substitute("$0$1", kMetadataColumnPrefix, col_name);
  }
  inline static std::string GetMetadataString(metadatapb::MetadataType metadata_type) {
    if (metadata_type == metadatapb::MetadataType::UPID) {
      return kUniquePIDColumn;
    }
    std::string name = metadatapb::MetadataType_Name(metadata_type);
    absl::AsciiStrToLower(&name);
    return name;
  }
  inline static std::string FormatMetadataColumn(metadatapb::MetadataType metadata_type) {
    if (metadata_type == metadatapb::MetadataType::UPID) {
      return kUniquePIDColumn;
    }
    return FormatMetadataColumn(GetMetadataString(metadata_type));
  }

  /**
   * @brief Strips the Metadata prefix from a given Carnot, column-name representation.
   * If you pass in a column without the prefix, it _does not_ throw an error.
   * @param column_name
   */
  inline static std::string ExtractMetadataFromColumnName(const std::string_view column_name) {
    return std::string(absl::StripPrefix(column_name, kMetadataColumnPrefix));
  }

  /**
   * @brief The metadata prefix for columns in Carnot.
   */
  inline static const char kMetadataColumnPrefix[] = "_attr_";
  /**
   * @brief The string representation of the unique pid column.
   */
  inline static constexpr char kUniquePIDColumn[] = "upid";

 protected:
  MetadataProperty(metadatapb::MetadataType metadata_type, types::DataType column_type,
                   std::vector<metadatapb::MetadataType> key_columns)
      : metadata_type_(metadata_type), column_type_(column_type), key_columns_(key_columns) {
    name_ = GetMetadataString(metadata_type);
  }

 private:
  metadatapb::MetadataType metadata_type_;
  types::DataType column_type_;
  std::string name_;
  std::vector<metadatapb::MetadataType> key_columns_;
};

class DataIR : public ExpressionIR {
 public:
  types::DataType EvaluatedDataType() const override { return evaluated_data_type_; }
  bool IsDataTypeEvaluated() const override { return true; }
  bool IsData() const override { return true; }

 protected:
  DataIR(int64_t id, IRNodeType type, types::DataType data_type)
      : ExpressionIR(id, type), evaluated_data_type_(data_type) {}

 private:
  types::DataType evaluated_data_type_;
};

/**
 * @brief ColumnIR wraps around columns in the plan.
 *
 * Columns have two important relationships that can easily get confused with one another.
 *
 * _Relationships_:
 * 1. A column is contained by an Operator.
 * 2. A column has an Operator that it references.
 *
 * The first relationship applies to Operators that utilize expressions (ie Map, Agg). Any
 * ColumnIR that is a progeny of an expression in an Operator means the Operator contains that
 * ColumnIR.
 *
 * The second relationship is between the ColumnIR and the parent of the Containing Operator (in
 * relationship #1) that the ColumnIR refers to. A ColumnIR that references an operator means it's
 * value is determined by the Operator and subsequently the Operator's output relation must
 * contain the column name used by this column.
 *
 * Because we are constantly shuffling parents of each Operator, ColumnIR's
 * methods are meant to be decoupled from the Operator that it references. Instead, ColumnIR
 * finds the referenced operator by first getting the containing operator, then grabbing the
 * saved index of that containing operator to point to a parent of that operator. This is under the
 * assumption that once we initialize an operator, the number of parents it has never changes.
 *
 * To get the Referenced Operator for a column, the Column accesses its saved index of the
 * Containing Operator's parents vector.
 *
 */
class ColumnIR : public ExpressionIR {
 public:
  ColumnIR() = delete;
  explicit ColumnIR(int64_t id) : ExpressionIR(id, IRNodeType::kColumn) {}
  /**
   * @brief Creates a new column that references the passed in operator.
   *
   * @param col_name: The name of the column.
   * @param parent_op_idx: The index of the parent operator that this column references.
   * @param ast_node: AST node used for query error reporting and debugging.
   * @return Status: error container.
   */
  Status Init(const std::string& col_name, int64_t parent_op_idx, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string col_name() const { return col_name_; }

  bool IsColumn() const override { return true; }
  void ResolveColumn(int64_t col_idx, types::DataType type) {
    col_idx_ = col_idx;
    evaluated_data_type_ = type;
    is_data_type_evaluated_ = true;
  }

  /**
   * @brief The operator that this column references. This should be the parent of the operator that
   * contains this column.
   *
   * @return OperatorIR*: the operator this column references.
   */
  StatusOr<OperatorIR*> ReferencedOperator() const;

  // TODO(philkuz) (PL-826): redo DebugString to have a DebugStringImpl instead of override.
  // NOLINTNEXTLINE(readability/inheritance)
  virtual std::string DebugString() const override;
  StatusOr<int64_t> ReferenceID() const {
    PL_ASSIGN_OR_RETURN(OperatorIR * referenced_op, ReferencedOperator());
    return referenced_op->id();
  }
  types::DataType EvaluatedDataType() const override { return evaluated_data_type_; }
  bool IsDataTypeEvaluated() const override { return is_data_type_evaluated_; }

  int64_t col_idx() const { return col_idx_; }
  int64_t container_op_parent_idx() const { return container_op_parent_idx_; }
  bool container_op_parent_idx_set() const { return container_op_parent_idx_set_; }

  /**
   * @brief Override DeepCloneInto to make sure all Column classes save the column attributes.
   *
   * @param graph
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> DeepCloneInto(IR* graph) const override;

  void SetContainingOperatorParentIdx(int64_t container_op_parent_idx);

 protected:
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;
  /**
   * @brief Optional protected constructor for children types.
   */
  ColumnIR(int64_t id, IRNodeType type) : ExpressionIR(id, type) {}

  /**
   * @brief Point to the index of the Containing operator's parents that this column references.
   *
   * @param container_op_parent_idx: the index of the container_op.
   */

  void SetColumnName(const std::string& col_name) {
    col_name_ = col_name;
    col_name_set_ = true;
  }

 private:
  std::string col_name_;
  bool col_name_set_ = false;
  // The column index in the relation.
  int64_t col_idx_;
  types::DataType evaluated_data_type_;
  bool is_data_type_evaluated_ = false;

  int64_t container_op_parent_idx_ = -1;
  bool container_op_parent_idx_set_ = false;
};

/**
 * @brief StringIR wraps around the String AST node
 * and only contains the value of that string.
 *
 */
class StringIR : public DataIR {
 public:
  StringIR() = delete;
  explicit StringIR(int64_t id) : DataIR(id, IRNodeType::kString, types::DataType::STRING) {}
  Status Init(std::string str, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string str() const { return str_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  std::string str_;
};

/**
 * @brief ListIR wraps around lists. Will maintain a
 * vector of pointers to the contained nodes in the
 * list.
 *
 */
class ListIR : public DataIR {
 public:
  ListIR() = delete;
  explicit ListIR(int64_t id) : DataIR(id, IRNodeType::kList, types::DataType::DATA_TYPE_UNKNOWN) {}
  bool HasLogicalRepr() const override;
  Status Init(const pypa::AstPtr& ast_node, std::vector<ExpressionIR*> children);

  std::vector<ExpressionIR*> children() const { return children_; }
  bool IsOperator() const override { return false; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  std::vector<ExpressionIR*> children_;
};

struct ColumnExpression {
  ColumnExpression(std::string col_name, ExpressionIR* expr) : name(col_name), node(expr) {}
  std::string name;
  ExpressionIR* node;
};
using ColExpressionVector = std::vector<ColumnExpression>;

/**
 * @brief IR representation for a Lambda
 * function. Should contain an expected
 * Relation based on which columns are called
 * within the contained relation.
 *
 */
class LambdaIR : public IRNode {
 public:
  LambdaIR() = delete;
  explicit LambdaIR(int64_t id) : IRNode(id, IRNodeType::kLambda, false) {}
  Status Init(std::unordered_set<std::string> column_names, const ColExpressionVector& col_exprs,
              const pypa::AstPtr& ast_node);
  /**
   * @brief Init for the Lambda called elsewhere. Uses a default value for the key to the
   * expression map.
   */
  Status Init(std::unordered_set<std::string> expected_column_names, ExpressionIR* node,
              const pypa::AstPtr& ast_node);
  /**
   * @brief Returns the one_expr_ if it has only one expr in the col_expr_map, otherwise returns
   * an error.
   *
   * @return StatusOr<IRNode*>
   */
  StatusOr<ExpressionIR*> GetDefaultExpr();
  bool HasLogicalRepr() const override;
  bool HasDictBody() const;

  bool IsOperator() const override { return false; }
  bool IsExpression() const override { return false; }
  std::unordered_set<std::string> expected_column_names() const { return expected_column_names_; }
  ColExpressionVector col_exprs() const { return col_exprs_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  static constexpr const char* default_key = "_default";
  std::unordered_set<std::string> expected_column_names_;
  ColExpressionVector col_exprs_;
  bool has_dict_body_;
};

/**
 * @brief Represents functions with arbitrary number of values
 */
class FuncIR : public ExpressionIR {
 public:
  enum Opcode {
    non_op = -1,
    mult,
    sub,
    add,
    div,
    eq,
    neq,
    lteq,
    gteq,
    lt,
    gt,
    logand,
    logor,
    mod,
    number_of_ops
  };
  struct Op {
    Opcode op_code;
    std::string python_op;
    std::string carnot_op_name;
  };
  static std::unordered_map<std::string, Op> op_map;

  FuncIR() = delete;
  Opcode opcode() const { return op_.op_code; }
  const Op& op() const { return op_; }
  explicit FuncIR(int64_t id) : ExpressionIR(id, IRNodeType::kFunc) {}
  Status Init(Op op, std::string func_prefix, const std::vector<ExpressionIR*>& args,
              bool compile_time, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;

  // NOLINTNEXTLINE(readability/inheritance)
  virtual std::string DebugString() const override {
    return absl::Substitute("$0($1)", func_name(),
                            absl::StrJoin(args_, ",", [](std::string* out, IRNode* in) {
                              absl::StrAppend(out, in->type_string());
                            }));
  }

  std::string func_name() const {
    return absl::Substitute("$0.$1", func_prefix_, op_.carnot_op_name);
  }
  int64_t func_id() const { return func_id_; }
  void set_func_id(int64_t func_id) { func_id_ = func_id; }
  const std::vector<ExpressionIR*>& args() { return args_; }
  const std::vector<types::DataType>& args_types() { return args_types_; }
  void SetArgsTypes(std::vector<types::DataType> args_types) { args_types_ = args_types; }
  // TODO(philkuz) figure out how to combine this with set_func_id.
  void SetOutputDataType(types::DataType type) {
    evaluated_data_type_ = type;
    is_data_type_evaluated_ = true;
  }
  Status UpdateArg(int64_t idx, ExpressionIR* arg) {
    CHECK_LT(idx, static_cast<int64_t>(args_.size()))
        << "Tried to update arg of index greater than number of args.";
    ExpressionIR* old_arg = args_[idx];
    args_[idx] = arg;
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(id(), old_arg->id()));
    return Status::OK();
  }
  types::DataType EvaluatedDataType() const override { return evaluated_data_type_; }
  bool IsDataTypeEvaluated() const override { return is_data_type_evaluated_; }

  bool is_compile_time() const { return is_compile_time_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  std::string func_prefix_;
  Op op_;
  std::string func_name_;
  std::vector<ExpressionIR*> args_;
  std::vector<types::DataType> args_types_;
  int64_t func_id_ = 0;
  types::DataType evaluated_data_type_ = types::DataType::DATA_TYPE_UNKNOWN;
  bool is_data_type_evaluated_ = false;
  bool is_compile_time_ = false;
};

/**
 * @brief Primitive values.
 */
class FloatIR : public DataIR {
 public:
  FloatIR() = delete;
  explicit FloatIR(int64_t id) : DataIR(id, IRNodeType::kFloat, types::DataType::FLOAT64) {}
  Status Init(double val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;

  double val() const { return val_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  double val_;
};

class IntIR : public DataIR {
 public:
  IntIR() = delete;
  explicit IntIR(int64_t id) : DataIR(id, IRNodeType::kInt, types::DataType::INT64) {}
  Status Init(int64_t val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;

  int64_t val() const { return val_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  int64_t val_;
};

class BoolIR : public DataIR {
 public:
  BoolIR() = delete;
  explicit BoolIR(int64_t id) : DataIR(id, IRNodeType::kBool, types::DataType::BOOLEAN) {}
  Status Init(bool val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;

  bool val() const { return val_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  bool val_;
};

class TimeIR : public DataIR {
 public:
  TimeIR() = delete;
  explicit TimeIR(int64_t id) : DataIR(id, IRNodeType::kTime, types::DataType::TIME64NS) {}
  Status Init(int64_t val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;

  bool val() const { return val_ != 0; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  int64_t val_;
};

class MetadataResolverIR;

class MetadataIR : public ColumnIR {
 public:
  MetadataIR() = delete;
  explicit MetadataIR(int64_t id) : ColumnIR(id, IRNodeType::kMetadata) {}
  Status Init(const std::string& metadata_val, int64_t parent_op_idx, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override { return false; };

  std::string name() const { return metadata_name_; }
  bool HasMetadataResolver() const { return has_metadata_resolver_; }

  Status ResolveMetadataColumn(MetadataResolverIR* resolver_op, MetadataProperty* property);
  MetadataResolverIR* resolver() const { return resolver_; }
  MetadataProperty* property() const { return property_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;
  std::string DebugString() const override;

 private:
  std::string metadata_name_;
  bool has_metadata_resolver_ = false;
  MetadataResolverIR* resolver_;
  MetadataProperty* property_;
};

/**
 * @brief MetadataLiteral is a representation for any literal (DataIR)
 * that we know is properly formatted for some function.
 */
class MetadataLiteralIR : public ExpressionIR {
 public:
  MetadataLiteralIR() = delete;
  explicit MetadataLiteralIR(int64_t id) : ExpressionIR(id, IRNodeType::kMetadataLiteral) {}
  Status Init(DataIR* literal, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override { return false; }

  IRNodeType literal_type() const { return literal_type_; }
  DataIR* literal() const { return literal_; }
  bool IsDataTypeEvaluated() const override { return literal_->IsDataTypeEvaluated(); }
  types::DataType EvaluatedDataType() const override { return literal_->EvaluatedDataType(); }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  DataIR* literal_;
  IRNodeType literal_type_;
};

/**
 * @brief The MemorySourceIR is a dual logical plan
 * and IR node operator. It inherits from both classes
 */
class MemorySourceIR : public OperatorIR {
 public:
  using OperatorIR::Init;
  MemorySourceIR() = delete;
  explicit MemorySourceIR(int64_t id)
      : OperatorIR(id, IRNodeType::kMemorySource, /* has_parents */ false, /* is_source */ true) {}
  bool HasLogicalRepr() const override;

  std::string table_name() { return table_name_; }
  void SetTime(int64_t time_start_ns, int64_t time_stop_ns) {
    time_start_ns_ = time_start_ns;
    time_stop_ns_ = time_stop_ns;
    time_set_ = true;
  }
  int64_t time_start_ns() const { return time_start_ns_; }
  int64_t time_stop_ns() const { return time_stop_ns_; }
  bool IsTimeSet() const { return time_set_; }
  bool columns_set() const { return columns_set_; }
  Status SetColumns(std::vector<ColumnIR*> columns) {
    columns_set_ = true;
    columns_ = columns;
    for (auto col : columns) {
      PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(id(), col->id()));
    }
    return Status::OK();
  }

  Status ToProto(planpb::Operator*) const override;
  std::vector<std::string> ArgKeys() override { return {"table", "select"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>{{"select", nullptr}};
  }
  Status InitImpl(const ArgMap& args) override;
  bool select_all() const { return column_names_.size() == 0; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;
  const std::vector<std::string>& column_names() const { return column_names_; }
  StatusOr<std::vector<std::string>> ParseStringListIR(const ListIR& list_ir);

 private:
  std::string table_name_;

  bool time_set_ = false;
  int64_t time_start_ns_;
  int64_t time_stop_ns_;
  // in conjunction with the relation, we can get the idx, names, and types of this column.
  std::vector<ColumnIR*> columns_;
  std::vector<std::string> column_names_;
  bool columns_set_ = false;
};

/**
 * The MemorySinkIR describes the MemorySink operator.
 */
class MemorySinkIR : public OperatorIR {
 public:
  MemorySinkIR() = delete;
  explicit MemorySinkIR(int64_t id)
      : OperatorIR(id, IRNodeType::kMemorySink, /* has_parents */ true, /* is_source */ false) {}
  bool HasLogicalRepr() const override;

  bool name_set() const { return name_set_; }
  std::string name() const { return name_; }
  Status ToProto(planpb::Operator*) const override;

  std::vector<std::string> ArgKeys() override { return {"name"}; }
  Status InitImpl(const ArgMap& args) override;

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;
  bool IsBlocking() const override { return true; }

 private:
  std::string name_;
  bool name_set_ = false;
};

/**
 * @brief The RangeIR describe the range()
 * operator, which is combined with a Source
 * when converted to the Logical Plan.
 *
 */
class RangeIR : public OperatorIR {
 public:
  RangeIR() = delete;
  explicit RangeIR(int64_t id) : OperatorIR(id, IRNodeType::kRange, true, false) {}
  Status Init(OperatorIR* parent, IRNode* start_repr, IRNode* stop_repr,
              const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;

  IRNode* start_repr() { return start_repr_; }
  IRNode* stop_repr() { return stop_repr_; }
  Status SetStartStop(IRNode* start_repr, IRNode* stop_repr);
  Status ToProto(planpb::Operator*) const override;

  // TODO(philkuz) implement
  std::vector<std::string> ArgKeys() override { return {"start", "stop"}; }

  // TODO(philkuz) implement
  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  // TODO(philkuz) implement
  Status InitImpl(const ArgMap& args) override;
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  // Start and Stop eventually evaluate to integers, but might be expressions.
  IRNode* start_repr_ = nullptr;
  IRNode* stop_repr_ = nullptr;
};

/**
 * @brief The MetadataResolverIR is a IR-only operation that
 * adds metadata as a column into the query.
 * At the end of the analyzer stage of the compiler this becomes a map node.
 */
class MetadataResolverIR : public OperatorIR {
 public:
  MetadataResolverIR() = delete;
  explicit MetadataResolverIR(int64_t id)
      : OperatorIR(id, IRNodeType::kMetadataResolver, true, false) {}
  Status InitImpl(const ArgMap& args) override;
  bool HasLogicalRepr() const override { return false; }
  Status ToProto(planpb::Operator*) const override {
    return error::Unimplemented("Calling ToProto on $0, which lacks a Protobuf representation.",
                                type_string());
  }

  std::vector<std::string> ArgKeys() override { return {}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }

  Status AddMetadata(MetadataProperty* md_property);
  bool HasMetadataColumn(const std::string& type);
  std::map<std::string, MetadataProperty*> metadata_columns() const { return metadata_columns_; }

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  std::map<std::string, MetadataProperty*> metadata_columns_;
};

/**
 * @brief The MapIR is a container for Map operators.
 * Describes a projection, which is describe in col_exprs().
 */
class MapIR : public OperatorIR {
 public:
  MapIR() = delete;
  explicit MapIR(int64_t id) : OperatorIR(id, IRNodeType::kMap, true, false) {}
  std::vector<std::string> ArgKeys() override { return {"fn"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  Status InitImpl(const ArgMap& args) override;

  bool HasLogicalRepr() const override;

  const ColExpressionVector& col_exprs() const { return col_exprs_; }
  Status ToProto(planpb::Operator*) const override;

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  Status SetupMapExpressions(LambdaIR* map_func);
  // The map from new column_names to expressions.
  ColExpressionVector col_exprs_;
};

/**
 * @brief The BlockingAggIR is the IR representation for the Agg operator.
 * GroupBy groups() and Aggregate columns according to aggregate_expressions().
 */
class BlockingAggIR : public OperatorIR {
 public:
  BlockingAggIR() = delete;
  explicit BlockingAggIR(int64_t id) : OperatorIR(id, IRNodeType::kBlockingAgg, true, false) {}
  bool HasLogicalRepr() const override;

  std::vector<ColumnIR*> groups() const { return groups_; }
  bool group_by_all() const { return groups_.size() == 0; }
  ColExpressionVector aggregate_expressions() const { return aggregate_expressions_; }
  Status ToProto(planpb::Operator*) const override;
  Status EvaluateAggregateExpression(planpb::AggregateExpression* expr,
                                     const IRNode& ir_node) const;

  std::vector<std::string> ArgKeys() override { return {"fn", "by"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>{{"by", nullptr}};
  }
  Status InitImpl(const ArgMap& args) override;

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;
  inline bool IsBlocking() const override { return true; }

 private:
  Status SetupGroupBy(LambdaIR* by_lambda);
  Status SetupAggFunctions(LambdaIR* agg_lambda);

  // contains group_names and groups columns.
  std::vector<ColumnIR*> groups_;
  // The map from value_names to values
  ColExpressionVector aggregate_expressions_;
};

class FilterIR : public OperatorIR {
 public:
  FilterIR() = delete;
  explicit FilterIR(int64_t id) : OperatorIR(id, IRNodeType::kFilter, true, false) {}
  bool HasLogicalRepr() const override;

  ExpressionIR* filter_expr() const { return filter_expr_; }
  Status ToProto(planpb::Operator*) const override;

  std::vector<std::string> ArgKeys() override { return {"fn"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  Status InitImpl(const ArgMap& args) override;

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  ExpressionIR* filter_expr_ = nullptr;
};

class LimitIR : public OperatorIR {
 public:
  LimitIR() = delete;
  explicit LimitIR(int64_t id) : OperatorIR(id, IRNodeType::kLimit, true, false) {}
  bool HasLogicalRepr() const override;

  Status ToProto(planpb::Operator*) const override;
  void SetLimitValue(int64_t value) {
    limit_value_ = value;
    limit_value_set_ = true;
  }
  bool limit_value_set() const { return limit_value_set_; }
  int64_t limit_value() const { return limit_value_; }

  std::vector<std::string> ArgKeys() override { return {"rows"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  Status InitImpl(const ArgMap& args) override;

  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  int64_t limit_value_;
  bool limit_value_set_ = false;
};

/**
 * @brief IR for the network sink operator that passes batches over GRPC to the destination.
 *
 * Setting up the Sink Operator requires three steps.
 * 0. Init(int destination_id): Set the destination id.
 * 1. SetPhysicalID(string): Set the name of the node same as the query broker.
 * 2. SetDestinationAddress(string): the GRPC address where batches should be sent.
 */
class GRPCSinkIR : public OperatorIR {
 public:
  explicit GRPCSinkIR(int64_t id)
      : OperatorIR(id, IRNodeType::kGRPCSink, /* has_parents */ true,
                   /* is_source */ false) {}
  bool HasLogicalRepr() const override { return true; }
  std::vector<std::string> ArgKeys() override { return {}; }
  Status InitImpl(const ArgMap&) override { return Status::OK(); }
  Status Init(OperatorIR* parent, int64_t destination_id, pypa::AstPtr ast_node) {
    destination_id_ = destination_id;
    return OperatorIR::Init(parent, {{}}, ast_node);
  }
  Status ToProto(planpb::Operator* op_pb) const override;

  /**
   * @brief The id used for initial mapping. This associates a GRPCSink with a subsequent
   * GRPCSourceGroup.
   *
   * Once the Physical Plan is established, you should use PhysicalDestinationID().
   */
  int64_t destination_id() const { return destination_id_; }

  /**
   * @brief Set the Physical ID of this node. This should be the same
   * string used to map mesage
   *
   * @param physical_source_id
   */
  void SetPhysicalID(const std::string& physical_id) { physical_id_ = physical_id; }

  bool PhysicalIDSet() const { return physical_id_ != ""; }

  /**
   * @brief An id that is used to map the batches from this sink when received at a source.
   *
   * @return std::string
   */
  std::string PhysicalDestinationID() const {
    return absl::Substitute("$0:$1", physical_id_, destination_id_);
  }
  void SetDestinationAddress(const std::string& address) { destination_address_ = address; }

  const std::string& destination_address() const { return destination_address_; }
  bool DestinationAddressSet() const { return destination_address_ != ""; }
  inline bool IsBlocking() const override { return true; }

 protected:
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  int64_t destination_id_ = -1;
  std::string physical_id_ = "";
  std::string destination_address_ = "";
};

/**
 * @brief This is the GRPC Source group that is what will appear in the physical plan.
 * Each GRPCSourceGroupIR will end up being converted into a set of GRPCSourceIR for the
 * corresponding remote_source_ids.
 */
class GRPCSourceIR : public OperatorIR {
 public:
  explicit GRPCSourceIR(int64_t id)
      : OperatorIR(id, IRNodeType::kGRPCSource, /* has_parents */ false,
                   /* is_source */ true) {}
  bool HasLogicalRepr() const override { return true; }
  std::vector<std::string> ArgKeys() override { return {}; }
  Status ToProto(planpb::Operator* op_pb) const override;
  Status InitImpl(const ArgMap&) override { return Status::OK(); }

  /**
   * @brief Special Init that skips around the Operator init function.
   *
   * @param source_id
   * @param ast_node
   * @return Status
   */
  Status Init(const std::string& remote_source_id, const Relation& relation,
              pypa::AstPtr ast_node) {
    remote_source_id_ = remote_source_id;
    PL_RETURN_IF_ERROR(SetRelation(relation));
    return OperatorIR::Init(nullptr, {{}}, ast_node);
  }

  const std::string& remote_source_id() const { return remote_source_id_; }

 protected:
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  // The id to map any incoming batches to this node.
  std::string remote_source_id_;
};

/**
 * @brief This is an IR only node that is used to mark where a GRPC source should go.
 * For the actual plan, this operator is replaced with a series of GRPCSourceOperators that are
 * Unioned together.
 */
class GRPCSourceGroupIR : public OperatorIR {
 public:
  explicit GRPCSourceGroupIR(int64_t id)
      : OperatorIR(id, IRNodeType::kGRPCSourceGroup, /* has_parents */ false,
                   /* is_source */ true) {}
  bool HasLogicalRepr() const override { return false; }
  std::vector<std::string> ArgKeys() override { return {}; }
  Status ToProto(planpb::Operator* op_pb) const override;

  Status InitImpl(const ArgMap&) override { return Status::OK(); }

  /**
   * @brief Special Init that skips around the Operator init function.
   *
   * @param source_id
   * @param ast_node
   * @return Status
   */
  Status Init(int64_t source_id, const Relation& relation, pypa::AstPtr ast_node) {
    source_id_ = source_id;
    PL_RETURN_IF_ERROR(SetRelation(relation));
    return OperatorIR::Init(nullptr, {{}}, ast_node);
  }

  void SetGRPCAddress(const std::string& grpc_address) { grpc_address_ = grpc_address; }

  /**
   * @brief Associate the passed in GRPCSinkOperator with this Source Group. The sink_op passed in
   * will most likely exist outside of the graph this contains, so instead of holding a pointer, we
   * hold the information that will be used during execution in Vizier.
   *
   * @param sink_op: the sink operator that should be connected with this source operator.
   * @return Status: error if this->source_id and sink_op->destination_id don't line up.
   */
  Status AddGRPCSink(GRPCSinkIR* sink_op);
  const std::vector<std::string>& remote_string_ids() const { return remote_string_ids_; }
  bool GRPCAddressSet() const { return grpc_address_ != ""; }
  const std::string& grpc_address() const { return grpc_address_; }
  int64_t source_id() const { return source_id_; }

 protected:
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

 private:
  int64_t source_id_ = -1;
  std::vector<std::string> remote_string_ids_;
  std::string grpc_address_ = "";
};

struct ColumnMapping {
  std::vector<int64_t> input_column_map;
};
class UnionIR : public OperatorIR {
 public:
  UnionIR() = delete;
  explicit UnionIR(int64_t id)
      : OperatorIR(id, IRNodeType::kUnion, /* has_parents */ true, /* is_source */ false) {}
  bool HasLogicalRepr() const override { return true; }

  std::vector<std::string> ArgKeys() override { return {}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }

  bool IsBlocking() const override { return true; }

  Status ToProto(planpb::Operator*) const override;
  // TODO(philkuz) figure out whether we need to do anything special to init the union operator.
  Status InitImpl(const ArgMap&) override { return Status::OK(); }
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;
  Status SetRelationFromParents();
  const std::vector<ColumnMapping>& column_mappings() const { return column_mappings_; }

 private:
  /**
   * @brief Set a parent operator's column mapping.
   *
   * @param parent_idx: the parents() idx this mapping applies to.
   * @param input_column_map: the mapping from 1 columns unions to another. [4] would indicate the
   * index-4 column of the input parent relation maps to index 0 of this relation.
   * @return Status
   */
  Status AddColumnMapping(const std::vector<int64_t>& input_column_map);
  std::vector<ColumnMapping> column_mappings_;
};

/**
 * @brief The Join Operator plan node.
 *
 */
class JoinIR : public OperatorIR {
 public:
  JoinIR() = delete;
  explicit JoinIR(int64_t id)
      : OperatorIR(id, IRNodeType::kJoin, /* has_parents */ true, /* is_source */ false) {}
  bool HasLogicalRepr() const override { return true; }

  std::vector<std::string> ArgKeys() override { return {"type", "cond", "cols"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>{{"type", nullptr}};
  }

  bool IsBlocking() const override { return true; }

  Status ToProto(planpb::Operator*) const override;
  Status InitImpl(const ArgMap&) override;
  StatusOr<IRNode*> DeepCloneIntoImpl(IR* graph) const override;

  struct EqualityCondition {
    int64_t left_column_idx;
    int64_t right_column_idx;
  };

  void AddEqualityCondition(int64_t left_idx, int64_t right_idx) {
    equality_conditions_.push_back(EqualityCondition({left_idx, right_idx}));
  }

  const std::vector<EqualityCondition>& equality_conditions() const {
    DCHECK_GT(equality_conditions_.size(), 0UL) << "Equality conditions must be created";
    return equality_conditions_;
  }

  bool HasEqualityConditions() const { return equality_conditions_.size() != 0; }

  FuncIR* condition_expr() const { return condition_expr_; }
  planpb::JoinOperator::JoinType join_type() const { return join_type_; }
  const std::vector<ColumnIR*>& output_columns() const { return output_columns_; }
  const std::vector<std::string>& column_names() const { return column_names_; }

 private:
  Status SetupConditionFromLambda(LambdaIR* condition);
  Status SetupOutputColumns(LambdaIR* output_columns);
  Status SetupJoinType(StringIR* join_type);
  /**
   * @brief Swaps the parents in the case. Used in the case this is a right join.
   *
   */
  Status FlipParents();

  planpb::JoinOperator::JoinType join_type_;
  // The condition expression that is eventually translated into equality_conditions_.
  FuncIR* condition_expr_;
  std::vector<EqualityCondition> equality_conditions_;
  std::vector<ColumnIR*> output_columns_;
  std::vector<std::string> column_names_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
