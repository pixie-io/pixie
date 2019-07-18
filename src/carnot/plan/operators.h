#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/plan/plan_node.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace plan {

// Forward declare to break deps.
class PlanState;

/**
 * Operator is the pure virtual base class for logical plan nodes that perform
 * operations on data/metadata.
 */
class Operator : public PlanNode {
 public:
  ~Operator() override = default;

  // Create a new operator using the Operator proto.
  static std::unique_ptr<Operator> FromProto(const planpb::Operator& pb, int64_t id);

  // Returns the ID of the operator.
  int64_t id() const { return id_; }

  // Returns the type of the operator.
  planpb::OperatorType op_type() const { return op_type_; }

  bool is_initialized() const { return is_initialized_; }

  // Generate a string that will help debug operators.
  std::string DebugString() const override = 0;

  // Prints out the debug to INFO log.
  void Debug() { LOG(INFO) << DebugString(); }

  virtual StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const = 0;

 protected:
  Operator(int64_t id, planpb::OperatorType op_type) : id_(id), op_type_(op_type) {}

  int64_t id_;
  planpb::OperatorType op_type_;
  // Tracks if the operator has been fully initialized.
  bool is_initialized_ = false;
};

class MemorySourceOperator : public Operator {
 public:
  explicit MemorySourceOperator(int64_t id) : Operator(id, planpb::MEMORY_SOURCE_OPERATOR) {}
  ~MemorySourceOperator() override = default;
  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::MemorySourceOperator& pb);
  std::string DebugString() const override;
  std::string TableName() const { return pb_.name(); }
  bool HasStartTime() const { return pb_.has_start_time(); }
  bool HasStopTime() const { return pb_.has_stop_time(); }
  int64_t start_time() const { return pb_.start_time().value(); }
  int64_t stop_time() const { return pb_.stop_time().value(); }
  std::vector<int64_t> Columns() const { return column_idxs_; }

 private:
  planpb::MemorySourceOperator pb_;

  std::vector<int64_t> column_idxs_;
};

class MapOperator : public Operator {
 public:
  explicit MapOperator(int64_t id) : Operator(id, planpb::MAP_OPERATOR) {}
  ~MapOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::MapOperator& pb);
  std::string DebugString() const override;

  const std::vector<std::shared_ptr<const ScalarExpression>>& expressions() const {
    return expressions_;
  }

 private:
  std::vector<std::shared_ptr<const ScalarExpression>> expressions_;
  std::vector<std::string> column_names_;

  planpb::MapOperator pb_;
};

class BlockingAggregateOperator : public Operator {
 public:
  explicit BlockingAggregateOperator(int64_t id)
      : Operator(id, planpb::BLOCKING_AGGREGATE_OPERATOR) {}
  ~BlockingAggregateOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::BlockingAggregateOperator& pb);
  std::string DebugString() const override;

  struct GroupInfo {
    std::string name;
    uint64_t idx;
  };

  const std::vector<GroupInfo>& groups() const { return groups_; }
  const std::vector<std::shared_ptr<AggregateExpression>>& values() const { return values_; }

 private:
  std::vector<std::shared_ptr<AggregateExpression>> values_;
  std::vector<GroupInfo> groups_;
  planpb::BlockingAggregateOperator pb_;
};

class MemorySinkOperator : public Operator {
 public:
  explicit MemorySinkOperator(int64_t id) : Operator(id, planpb::MEMORY_SINK_OPERATOR) {}
  ~MemorySinkOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::MemorySinkOperator& pb);
  std::string TableName() const { return pb_.name(); }
  std::string ColumnName(int64_t i) const { return pb_.column_names(i); }
  std::string DebugString() const override;

 private:
  planpb::MemorySinkOperator pb_;
};

class GrpcSourceOperator : public Operator {
 public:
  explicit GrpcSourceOperator(int64_t id) : Operator(id, planpb::GRPC_SOURCE_OPERATOR) {}
  ~GrpcSourceOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::GrpcSourceOperator& pb);
  std::string DebugString() const override;

  int64_t source_id() const { return pb_.source_id(); }

 private:
  planpb::GrpcSourceOperator pb_;
};

class GrpcSinkOperator : public Operator {
 public:
  explicit GrpcSinkOperator(int64_t id) : Operator(id, planpb::GRPC_SINK_OPERATOR) {}
  ~GrpcSinkOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::GrpcSinkOperator& pb);
  std::string DebugString() const override;

  std::string address() const { return pb_.address(); }
  int64_t destination_id() const { return pb_.destination_id(); }

 private:
  planpb::GrpcSinkOperator pb_;
};

class FilterOperator : public Operator {
 public:
  explicit FilterOperator(int64_t id) : Operator(id, planpb::FILTER_OPERATOR) {}
  ~FilterOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::FilterOperator& pb);
  std::string DebugString() const override;

  const std::shared_ptr<const ScalarExpression>& expression() const { return expression_; }

 private:
  std::shared_ptr<const ScalarExpression> expression_;

  planpb::FilterOperator pb_;
};

class LimitOperator : public Operator {
 public:
  explicit LimitOperator(int64_t id) : Operator(id, planpb::LIMIT_OPERATOR) {}
  ~LimitOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::LimitOperator& pb);
  std::string DebugString() const override;

  int64_t record_limit() const { return record_limit_; }

 private:
  int64_t record_limit_ = 0;

  planpb::LimitOperator pb_;
};

class UnionOperator : public Operator {
 public:
  explicit UnionOperator(int64_t id) : Operator(id, planpb::UNION_OPERATOR) {}
  ~UnionOperator() override = default;

  StatusOr<table_store::schema::Relation> OutputRelation(
      const table_store::schema::Schema& schema, const PlanState& state,
      const std::vector<int64_t>& input_ids) const override;
  Status Init(const planpb::UnionOperator& pb);
  std::string DebugString() const override;

  const std::vector<std::string>& column_names() const { return column_names_; }
  bool order_by_time() const { return time_column_indexes_.size() > 0; }
  const std::vector<int64_t>& time_column_indexes() const { return time_column_indexes_; }
  int64_t time_column_index(int64_t parent_index) const {
    return time_column_indexes_.at(parent_index);
  }
  const std::vector<std::vector<int64_t>>& column_mappings() const { return column_mappings_; }
  const std::vector<int64_t>& column_mapping(int64_t parent_index) const {
    return column_mappings_.at(parent_index);
  }

 private:
  std::vector<std::string> column_names_;
  std::vector<int64_t> time_column_indexes_;
  std::vector<std::vector<int64_t>> column_mappings_;

  planpb::UnionOperator pb_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
