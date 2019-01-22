#pragma once
#include <glog/logging.h>

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/plan/proto/plan.pb.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/utils/status.h"
#include "src/utils/statusor.h"

namespace pl {
namespace carnot {
namespace plan {

// Forward declare to break deps.
class CompilerState;
class Schema;
class Relation;

/**
 * Operator is the pure virtual base class for logical plan nodes that perform
 * operations on data/metadata.
 */
class Operator {
 public:
  virtual ~Operator() = default;

  // Create a new operator using the Operator proto.
  static std::unique_ptr<Operator> FromProto(const planpb::Operator &pb, int64_t id);

  // Returns the ID of the operator.
  int64_t id() const { return id_; }

  // Returns the type of the operator.
  planpb::OperatorType op_type() const { return op_type_; }

  bool is_initialized() const { return is_initialized_; }

  // Generate a string that will help debug operators.
  virtual std::string DebugString() = 0;

  // Prints out the debug to INFO log.
  void Debug() { LOG(INFO) << DebugString(); }

  virtual StatusOr<Relation> OutputRelation(const Schema &schema, const CompilerState &state,
                                            const std::vector<uint64_t> &input_ids) = 0;

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
  StatusOr<Relation> OutputRelation(const Schema &schema, const CompilerState &state,
                                    const std::vector<uint64_t> &input_ids) override;
  Status Init(const planpb::MemorySourceOperator &pb);
  std::string DebugString() override;

 private:
  planpb::MemorySourceOperator pb_;
};

class MapOperator : public Operator {
 public:
  explicit MapOperator(int64_t id) : Operator(id, planpb::MAP_OPERATOR) {}
  ~MapOperator() override = default;
  StatusOr<Relation> OutputRelation(const Schema &schema, const CompilerState &state,
                                    const std::vector<uint64_t> &input_ids) override;
  Status Init(const planpb::MapOperator &pb);
  std::string DebugString() override;

 private:
  std::vector<std::unique_ptr<ScalarExpression>> expressions_;
  std::vector<std::string> column_names_;

  planpb::MapOperator pb_;
};

class BlockingAggregateOperator : public Operator {
 public:
  explicit BlockingAggregateOperator(int64_t id)
      : Operator(id, planpb::BLOCKING_AGGREGATE_OPERATOR) {}
  ~BlockingAggregateOperator() override = default;
  StatusOr<Relation> OutputRelation(const Schema &schema, const CompilerState &state,
                                    const std::vector<uint64_t> &input_ids) override;
  Status Init(const planpb::BlockingAggregateOperator &pb);
  std::string DebugString() override;

 private:
  planpb::BlockingAggregateOperator pb_;
};

class MemorySinkOperator : public Operator {
 public:
  explicit MemorySinkOperator(int64_t id) : Operator(id, planpb::MEMORY_SINK_OPERATOR) {}
  ~MemorySinkOperator() override = default;
  StatusOr<Relation> OutputRelation(const Schema &schema, const CompilerState &state,
                                    const std::vector<uint64_t> &input_ids) override;
  Status Init(const planpb::MemorySinkOperator &pb);
  std::string DebugString() override;

 private:
  planpb::MemorySinkOperator pb_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
