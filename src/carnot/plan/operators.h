#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/plan/plan_node.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/proto/plan.pb.h"
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
  static std::unique_ptr<Operator> FromProto(const carnotpb::Operator &pb, int64_t id);

  // Returns the ID of the operator.
  int64_t id() const { return id_; }

  // Returns the type of the operator.
  carnotpb::OperatorType op_type() const { return op_type_; }

  bool is_initialized() const { return is_initialized_; }

  // Generate a string that will help debug operators.
  std::string DebugString() const override = 0;

  // Prints out the debug to INFO log.
  void Debug() { LOG(INFO) << DebugString(); }

  virtual StatusOr<schema::Relation> OutputRelation(
      const schema::Schema &schema, const PlanState &state,
      const std::vector<int64_t> &input_ids) const = 0;

 protected:
  Operator(int64_t id, carnotpb::OperatorType op_type) : id_(id), op_type_(op_type) {}

  int64_t id_;
  carnotpb::OperatorType op_type_;
  // Tracks if the operator has been fully initialized.
  bool is_initialized_ = false;
};

class MemorySourceOperator : public Operator {
 public:
  explicit MemorySourceOperator(int64_t id) : Operator(id, carnotpb::MEMORY_SOURCE_OPERATOR) {}
  ~MemorySourceOperator() override = default;
  StatusOr<schema::Relation> OutputRelation(const schema::Schema &schema, const PlanState &state,
                                            const std::vector<int64_t> &input_ids) const override;
  Status Init(const carnotpb::MemorySourceOperator &pb);
  std::string DebugString() const override;
  std::string TableName() const { return pb_.name(); }
  bool HasStartTime() const { return pb_.has_start_time(); }
  bool HasStopTime() const { return pb_.has_stop_time(); }
  int64_t start_time() const { return pb_.start_time().value(); }
  int64_t stop_time() const { return pb_.stop_time().value(); }
  std::vector<int64_t> Columns() const { return column_idxs_; }

 private:
  carnotpb::MemorySourceOperator pb_;

  std::vector<int64_t> column_idxs_;
};

class MapOperator : public Operator {
 public:
  explicit MapOperator(int64_t id) : Operator(id, carnotpb::MAP_OPERATOR) {}
  ~MapOperator() override = default;

  StatusOr<schema::Relation> OutputRelation(const schema::Schema &schema, const PlanState &state,
                                            const std::vector<int64_t> &input_ids) const override;
  Status Init(const carnotpb::MapOperator &pb);
  std::string DebugString() const override;

  const std::vector<std::shared_ptr<const ScalarExpression>> &expressions() const {
    return expressions_;
  }

 private:
  std::vector<std::shared_ptr<const ScalarExpression>> expressions_;
  std::vector<std::string> column_names_;

  carnotpb::MapOperator pb_;
};

class BlockingAggregateOperator : public Operator {
 public:
  explicit BlockingAggregateOperator(int64_t id)
      : Operator(id, carnotpb::BLOCKING_AGGREGATE_OPERATOR) {}
  ~BlockingAggregateOperator() override = default;

  StatusOr<schema::Relation> OutputRelation(const schema::Schema &schema, const PlanState &state,
                                            const std::vector<int64_t> &input_ids) const override;
  Status Init(const carnotpb::BlockingAggregateOperator &pb);
  std::string DebugString() const override;

  struct GroupInfo {
    std::string name;
    uint64_t idx;
  };

  const std::vector<GroupInfo> &groups() const { return groups_; }
  const std::vector<std::shared_ptr<AggregateExpression>> &values() const { return values_; }

 private:
  std::vector<std::shared_ptr<AggregateExpression>> values_;
  std::vector<GroupInfo> groups_;
  carnotpb::BlockingAggregateOperator pb_;
};

class MemorySinkOperator : public Operator {
 public:
  explicit MemorySinkOperator(int64_t id) : Operator(id, carnotpb::MEMORY_SINK_OPERATOR) {}
  ~MemorySinkOperator() override = default;

  StatusOr<schema::Relation> OutputRelation(const schema::Schema &schema, const PlanState &state,
                                            const std::vector<int64_t> &input_ids) const override;
  Status Init(const carnotpb::MemorySinkOperator &pb);
  std::string TableName() const { return pb_.name(); }
  std::string ColumnName(int64_t i) const { return pb_.column_names(i); }
  std::string DebugString() const override;

 private:
  carnotpb::MemorySinkOperator pb_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
