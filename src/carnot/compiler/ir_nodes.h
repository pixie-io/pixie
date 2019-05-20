#pragma once
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
struct ColumnExpression {
  std::string name;
  IRNode* node;
};
using ColExpressionVector = std::vector<ColumnExpression>;

enum IRNodeType {
  MemorySourceType,
  MemorySinkType,
  RangeType,
  MapType,
  BlockingAggType,
  FilterType,
  LimitType,
  StringType,
  FloatType,
  IntType,
  BoolType,
  FuncType,
  ListType,
  LambdaType,
  ColumnType,
  TimeType,
  number_of_types  // This is not a real type, but is used to verify strings are inline
                   // with enums.
};
static constexpr const char* kIRNodeStrings[] = {
    "MemorySourceType", "MemorySinkType", "RangeType",  "MapType", "BlockingAggType", "FilterType",
    "LimitType",        "StringType",     "FloatType",  "IntType", "BoolType",        "FuncType",
    "ListType",         "LambdaType",     "ColumnType", "TimeType"};

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
  virtual std::string DebugString(int64_t depth) const = 0;
  virtual bool IsOp() const = 0;
  bool is_source() const { return is_source_; }
  IRNodeType type() const { return type_; }
  std::string type_string() const { return kIRNodeStrings[type()]; }
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
  IR* graph_ptr() { return graph_ptr_; }
  pypa::AstPtr ast_node() const { return ast_node_; }
  /**
   * @brief Create an error that incorporates line, column of ir node into the error message.
   *
   * @param err_msg
   * @return Status
   */
  template <typename... Args>
  Status CreateIRNodeError(Args... args) const {
    compilerpb::CompilerErrorGroup context =
        LineColErrorPb(line(), col(), absl::Substitute(args...));
    return Status(statuspb::INVALID_ARGUMENT, "",
                  std::make_unique<compilerpb::CompilerErrorGroup>(context));
  }

 protected:
  explicit IRNode(int64_t id, IRNodeType type, bool is_source)
      : id_(id), type_(type), is_source_(is_source) {}
  void SetLineCol(int64_t line, int64_t col);
  void SetLineCol(const pypa::AstPtr& ast_node);

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
    auto id = id_node_map_.size();
    auto node = std::make_unique<TOperator>(id);
    dag_.AddNode(node->id());
    node->SetGraphPtr(this);
    TOperator* raw = node.get();
    id_node_map_.emplace(node->id(), std::move(node));
    return raw;
  }

  Status AddEdge(int64_t from_node, int64_t to_node);
  Status AddEdge(IRNode* from_node, IRNode* to_node);
  void DeleteEdge(int64_t from_node, int64_t to_node);
  void DeleteNode(int64_t node);
  plan::DAG& dag() { return dag_; }
  const plan::DAG& dag() const { return dag_; }
  std::string DebugString();
  IRNode* Get(int64_t id) const { return id_node_map_.at(id).get(); }
  size_t size() const { return id_node_map_.size(); }
  StatusOr<std::vector<IRNode*>> GetSinks() {
    std::vector<IRNode*> nodes;
    for (auto& i : dag().TopologicalSort()) {
      IRNode* node = Get(i);
      if (node->type() == MemorySinkType) {
        nodes.push_back(node);
        DCHECK(node->IsOp());
      }
    }
    if (nodes.empty()) {
      return error::InvalidArgument("No Result() found in the graph.");
    }
    return nodes;
  }

 private:
  plan::DAG dag_;
  std::unordered_map<int64_t, IRNodePtr> id_node_map_;
};

/**
 * @brief Node class for the operator
 *
 */
class OperatorIR : public IRNode {
 public:
  OperatorIR() = delete;
  bool IsOp() const override { return true; }
  table_store::schema::Relation relation() const { return relation_; }
  Status SetRelation(table_store::schema::Relation relation) {
    relation_init_ = true;
    relation_ = relation;
    return Status::OK();
  }
  bool IsRelationInit() const { return relation_init_; }
  bool HasParent() const { return has_parent_; }
  OperatorIR* parent() const { return parent_; }
  Status SetParent(IRNode* node);
  Status Init(IRNode* parent, const ArgMap& args, const pypa::AstPtr& ast_node);
  virtual Status InitImpl(const ArgMap& args) = 0;
  virtual std::vector<std::string> ArgKeys() = 0;
  virtual std::unordered_map<std::string, IRNode*> DefaultArgValues(
      const pypa::AstPtr& ast_node) = 0;
  virtual Status ToProto(planpb::Operator*) const = 0;
  // Checks whether the passed in arg maps contains the expected keys in this init function.
  Status ArgMapContainsKeys(const ArgMap& args);

  Status EvaluateExpression(planpb::ScalarExpression* expr, const IRNode& ir_node) const;

 protected:
  explicit OperatorIR(int64_t id, IRNodeType type, bool has_parent, bool is_source)
      : IRNode(id, type, is_source), has_parent_(has_parent) {}

 private:
  table_store::schema::Relation relation_;
  bool relation_init_ = false;
  bool has_parent_;
  OperatorIR* parent_;
};

/**
 * @brief ColumnIR wraps around columns found in the lambda functions.
 *
 */
class ColumnIR : public IRNode {
 public:
  ColumnIR() = delete;
  explicit ColumnIR(int64_t id) : IRNode(id, ColumnType, false) {}
  Status Init(const std::string& col_name, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string col_name() const { return col_name_; }
  std::string DebugString(int64_t depth) const override;
  bool IsOp() const override { return false; }
  void SetColumnIdx(int64_t col_idx) {
    col_idx_ = col_idx;
    col_idx_set_ = true;
  }
  bool col_idx_set() const { return col_idx_set_; }
  int64_t col_idx() const { return col_idx_; }

  void SetColumnType(types::DataType type) {
    type_ = type;
    col_type_set_ = true;
  }
  bool col_type_set() const { return col_type_set_; }
  types::DataType type() const { return type_; }

 private:
  std::string col_name_;
  // The column index in the relation.
  int64_t col_idx_;
  // The data type in the relation.
  types::DataType type_;
  bool col_idx_set_ = false;
  bool col_type_set_ = false;
};

/**
 * @brief StringIR wraps around the String AST node
 * and only contains the value of that string.
 *
 */
class StringIR : public IRNode {
 public:
  StringIR() = delete;
  explicit StringIR(int64_t id) : IRNode(id, StringType, false) {}
  Status Init(std::string str, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string str() const { return str_; }
  std::string DebugString(int64_t depth) const override;
  bool IsOp() const override { return false; }

 private:
  std::string str_;
};

/**
 * @brief ListIR wraps around lists. Will maintain a
 * vector of pointers to the contained nodes in the
 * list.
 *
 */
class ListIR : public IRNode {
 public:
  ListIR() = delete;
  explicit ListIR(int64_t id) : IRNode(id, ListType, false) {}
  bool HasLogicalRepr() const override;
  // TODO(philkuz) (PL-545) refactor lists
  Status Init(const pypa::AstPtr& ast_node, std::vector<IRNode*> children);
  std::string DebugString(int64_t depth) const override;
  std::vector<IRNode*> children() { return children_; }
  bool IsOp() const override { return false; }

 private:
  std::vector<IRNode*> children_;
};

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
  explicit LambdaIR(int64_t id) : IRNode(id, LambdaType, false) {}
  Status Init(std::unordered_set<std::string> column_names, const ColExpressionVector& col_exprs,
              const pypa::AstPtr& ast_node);
  /**
   * @brief Init for the Lambda called elsewhere. Uses a default value for the key to the expression
   * map.
   */
  Status Init(std::unordered_set<std::string> expected_column_names, IRNode* node,
              const pypa::AstPtr& ast_node);
  /**
   * @brief Returns the one_expr_ if it has only one expr in the col_expr_map, otherwise returns an
   * error.
   *
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> GetDefaultExpr();
  bool HasLogicalRepr() const override;
  bool HasDictBody() const;
  std::string DebugString(int64_t depth) const override;
  bool IsOp() const override { return false; }
  std::unordered_set<std::string> expected_column_names() const { return expected_column_names_; }
  ColExpressionVector col_exprs() const { return col_exprs_; }

 private:
  static constexpr const char* default_key = "_default";
  std::unordered_set<std::string> expected_column_names_;
  ColExpressionVector col_exprs_;
  bool has_dict_body_;
};

/**
 * @brief Represents functions with arbitrary number of values
 */
class FuncIR : public IRNode {
 public:
  FuncIR() = delete;
  explicit FuncIR(int64_t id) : IRNode(id, FuncType, false) {}
  Status Init(std::string func_name, const std::vector<IRNode*>& args,
              const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  std::string func_name() const { return func_name_; }
  int64_t func_id() const { return func_id_; }
  void set_func_id(int64_t func_id) { func_id_ = func_id; }
  const std::vector<IRNode*>& args() { return args_; }
  const std::vector<types::DataType>& args_types() { return args_types_; }
  void SetArgsTypes(std::vector<types::DataType> args_types) { args_types_ = args_types; }

  bool IsOp() const override { return false; }

 private:
  std::string func_name_;
  std::vector<IRNode*> args_;
  std::vector<types::DataType> args_types_;
  int64_t func_id_ = 0;
};

/**
 * @brief Primitive values.
 */
class FloatIR : public IRNode {
 public:
  FloatIR() = delete;
  explicit FloatIR(int64_t id) : IRNode(id, FloatType, false) {}
  Status Init(double val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  double val() const { return val_; }
  bool IsOp() const override { return false; }

 private:
  double val_;
};

class IntIR : public IRNode {
 public:
  IntIR() = delete;
  explicit IntIR(int64_t id) : IRNode(id, IntType, false) {}
  Status Init(int64_t val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  int64_t val() const { return val_; }
  bool IsOp() const override { return false; }

 private:
  int64_t val_;
};

class BoolIR : public IRNode {
 public:
  BoolIR() = delete;
  explicit BoolIR(int64_t id) : IRNode(id, BoolType, false) {}
  Status Init(bool val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  bool val() const { return val_; }
  bool IsOp() const override { return false; }

 private:
  bool val_;
};

class TimeIR : public IRNode {
 public:
  TimeIR() = delete;
  explicit TimeIR(int64_t id) : IRNode(id, TimeType, false) {}
  Status Init(int64_t val, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  bool val() const { return val_ != 0; }
  bool IsOp() const override { return false; }

 private:
  int64_t val_;
};

/**
 * @brief The MemorySourceIR is a dual logical plan
 * and IR node operator. It inherits from both classes
 */
class MemorySourceIR : public OperatorIR {
 public:
  MemorySourceIR() = delete;
  explicit MemorySourceIR(int64_t id) : OperatorIR(id, MemorySourceType, false, true) {}
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  std::string table_name() { return table_name_; }
  ListIR* select() { return select_; }
  void SetTime(int64_t time_start_ns, int64_t time_stop_ns) {
    time_start_ns_ = time_start_ns;
    time_stop_ns_ = time_stop_ns;
    time_set_ = true;
  }
  int64_t time_start_ns() const { return time_start_ns_; }
  int64_t time_stop_ns() const { return time_stop_ns_; }
  bool IsTimeSet() const { return time_set_; }
  bool columns_set() const { return columns_set_; }
  void SetColumns(std::vector<ColumnIR*> columns) {
    columns_set_ = true;
    columns_ = columns;
  }
  Status ToProto(planpb::Operator*) const override;
  std::vector<std::string> ArgKeys() override { return {"table", "select"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    // TODO(philkuz) allow default select value that selects all.
    return std::unordered_map<std::string, IRNode*>();
  }
  Status InitImpl(const ArgMap& args) override;

 private:
  std::string table_name_;
  ListIR* select_;
  bool time_set_ = false;
  int64_t time_start_ns_;
  int64_t time_stop_ns_;
  // in conjunction with the relation, we can get the idx, names, and types of this column.
  std::vector<ColumnIR*> columns_;
  bool columns_set_ = false;
};

/**
 * The MemorySinkIR describes the MemorySink operator.
 */
class MemorySinkIR : public OperatorIR {
 public:
  MemorySinkIR() = delete;
  explicit MemorySinkIR(int64_t id) : OperatorIR(id, MemorySinkType, true, false) {}
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  bool name_set() const { return name_set_; }
  std::string name() const { return name_; }
  Status ToProto(planpb::Operator*) const override;

  std::vector<std::string> ArgKeys() override { return {"name"}; }
  Status InitImpl(const ArgMap& args) override;

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }

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
  explicit RangeIR(int64_t id) : OperatorIR(id, RangeType, true, false) {}
  Status Init(IRNode* parent, IRNode* start_repr, IRNode* stop_repr, const pypa::AstPtr& ast_node);
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
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

 private:
  // Start and Stop eventually evaluate to integers, but might be expressions.
  IRNode* start_repr_ = nullptr;
  IRNode* stop_repr_ = nullptr;
};

// TODO(philkuz) adjust the documentation for map.
/**
 * @brief The RangeIR describe the range()
 * operator, which is combined with a Source
 * when converted to the Logical Plan.
 *
 */
class MapIR : public OperatorIR {
 public:
  MapIR() = delete;
  explicit MapIR(int64_t id) : OperatorIR(id, MapType, true, false) {}
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  LambdaIR* lambda_func() const { return lambda_func_; }
  void SetColExprs(ColExpressionVector col_exprs) {
    col_exprs_ = col_exprs;
    col_exprs_set_ = true;
  }
  bool col_exprs_set() const { return col_exprs_set_; }
  Status ToProto(planpb::Operator*) const override;

  std::vector<std::string> ArgKeys() override { return {"fn"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  Status InitImpl(const ArgMap& args) override;

 private:
  LambdaIR* lambda_func_;
  // The map from new column_names to expressions.
  ColExpressionVector col_exprs_;
  bool col_exprs_set_ = false;
};

// TODO(philkuz) adjust the documentation for blocking agg.
/**
 * @brief The RangeIR describe the range()
 * operator, which is combined with a Source
 * when converted to the Logical Plan.
 *
 */
class BlockingAggIR : public OperatorIR {
 public:
  BlockingAggIR() = delete;
  explicit BlockingAggIR(int64_t id) : OperatorIR(id, BlockingAggType, true, false) {}
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  LambdaIR* by_func() const { return by_func_; }
  LambdaIR* agg_func() const { return agg_func_; }
  void SetGroups(std::vector<ColumnIR*> groups) {
    groups_ = groups;
    groups_set_ = true;
  }
  std::vector<ColumnIR*> groups() const { return groups_; }
  bool groups_set() const { return groups_set_; }
  void SetAggValMap(ColExpressionVector agg_val_vec) {
    agg_val_vector_ = agg_val_vec;
    agg_val_vector_set_ = true;
  }
  bool agg_val_vector_set() const { return agg_val_vector_set_; }
  Status ToProto(planpb::Operator*) const override;
  Status EvaluateAggregateExpression(planpb::AggregateExpression* expr,
                                     const IRNode& ir_node) const;

  std::vector<std::string> ArgKeys() override { return {"fn", "by"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>{{"by", nullptr}};
  }
  Status InitImpl(const ArgMap& args) override;

 private:
  LambdaIR* by_func_;
  LambdaIR* agg_func_;
  // contains group_names and groups columns.
  std::vector<ColumnIR*> groups_;
  bool groups_set_ = false;
  // The map from value_names to values
  ColExpressionVector agg_val_vector_;
  bool agg_val_vector_set_ = false;
};

class FilterIR : public OperatorIR {
 public:
  FilterIR() = delete;
  explicit FilterIR(int64_t id) : OperatorIR(id, FilterType, true, false) {}
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
  LambdaIR* filter_func() const { return filter_func_; }
  Status ToProto(planpb::Operator*) const override;

  std::vector<std::string> ArgKeys() override { return {"fn"}; }

  std::unordered_map<std::string, IRNode*> DefaultArgValues(const pypa::AstPtr&) override {
    return std::unordered_map<std::string, IRNode*>();
  }
  Status InitImpl(const ArgMap& args) override;

 private:
  LambdaIR* filter_func_;
};

class LimitIR : public OperatorIR {
 public:
  LimitIR() = delete;
  explicit LimitIR(int64_t id) : OperatorIR(id, LimitType, true, false) {}
  bool HasLogicalRepr() const override;
  std::string DebugString(int64_t depth) const override;
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

 private:
  int64_t limit_value_;
  bool limit_value_set_ = false;
};

/**
 * A walker for an IR Graph.
 *
 * The walker walks through the operators of the graph in a topologically sorted order.
 */
class IRWalker {
 public:
  template <typename TOp>
  using NodeWalkFn = std::function<Status(const TOp&)>;

  using MemorySourceWalkFn = NodeWalkFn<MemorySourceIR>;
  using MapWalkFn = NodeWalkFn<MapIR>;
  using AggWalkFn = NodeWalkFn<BlockingAggIR>;
  using FilterWalkFn = NodeWalkFn<FilterIR>;
  using LimitWalkFn = NodeWalkFn<LimitIR>;
  using MemorySinkWalkFn = NodeWalkFn<MemorySinkIR>;

  /**
   * Register callback for when a memory source IR node is encountered.
   * @param fn The function to call when a memory source IR node is encountered.
   * @return self to allow chaining
   */
  IRWalker& OnMemorySource(const MemorySourceWalkFn fn) {
    memory_source_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a map IR node is encountered.
   * @param fn The function to call when a map IR node is encountered.
   * @return self to allow chaining.
   */
  IRWalker& OnMap(const MapWalkFn& fn) {
    map_walk_fn_ = fn;
    return *this;
  }
  /**
   * Register callback for when a filter IR node is encountered.
   * @param fn The function to call when a filter IR node is encountered.
   * @return self to allow chaining.
   */
  IRWalker& OnFilter(const FilterWalkFn& fn) {
    filter_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a limit IR node is encountered.
   * @param fn The function to call when a limit IR node is encountered.
   * @return self to allow chaining.
   */
  IRWalker& OnLimit(const LimitWalkFn& fn) {
    limit_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when an agg IR node is encountered.
   * @param fn The function to call when an agg IR node is encountered.
   * @return self to allow chaining.
   */
  IRWalker& OnBlockingAggregate(const AggWalkFn& fn) {
    agg_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a memory sink IR node is encountered.
   * @param fn The function to call.
   * @return self to allow chaining.
   */
  IRWalker& OnMemorySink(const MemorySinkWalkFn& fn) {
    memory_sink_walk_fn_ = fn;
    return *this;
  }

  /**
   * Perform a walk of the operators in the IR graph in a topologically-sorted order.
   * @param ir_graph The IR graph to walk.
   */
  Status Walk(const IR& ir_graph) {
    auto operators = ir_graph.dag().TopologicalSort();
    for (const auto& node_id : operators) {
      auto node = ir_graph.Get(node_id);
      if (node->IsOp()) {
        PL_RETURN_IF_ERROR(CallWalkFn(*node));
      }
    }
    return Status::OK();
  }

 private:
  template <typename T, typename TWalkFunc>
  Status CallAs(const TWalkFunc& fn, const IRNode& node) {
    if (!fn) {
      VLOG(google::WARNING) << "fn does not exist";
    }
    return fn(static_cast<const T&>(node));
  }

  Status CallWalkFn(const IRNode& node) {
    const auto op_type = node.type();
    switch (op_type) {
      case IRNodeType::MemorySourceType:
        return CallAs<MemorySourceIR>(memory_source_walk_fn_, node);
      case IRNodeType::MapType:
        return CallAs<MapIR>(map_walk_fn_, node);
      case IRNodeType::BlockingAggType:
        return CallAs<BlockingAggIR>(agg_walk_fn_, node);
      case IRNodeType::FilterType:
        return CallAs<FilterIR>(filter_walk_fn_, node);
      case IRNodeType::LimitType:
        return CallAs<LimitIR>(limit_walk_fn_, node);
      case IRNodeType::MemorySinkType:
        return CallAs<MemorySinkIR>(memory_sink_walk_fn_, node);
      case IRNodeType::RangeType:
        // Don't do anything with Range because we should have already combined Range with
        // MemorySource
        break;
      default:
        LOG(WARNING) << absl::StrFormat("IRNode %s does not exist for CallWalkFn",
                                        node.type_string());
    }
    return Status::OK();
  }

  MemorySourceWalkFn memory_source_walk_fn_;
  MapWalkFn map_walk_fn_;
  AggWalkFn agg_walk_fn_;
  FilterWalkFn filter_walk_fn_;
  LimitWalkFn limit_walk_fn_;
  MemorySinkWalkFn memory_sink_walk_fn_;
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
