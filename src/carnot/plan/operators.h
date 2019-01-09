#pragma once
#include <glog/logging.h>

#include <memory>
#include <string>

#include "src/carnot/plan/proto/plan.pb.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace plan {

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
  virtual ~MemorySourceOperator() {}

  Status Init(const planpb::MemorySourceOperator &pb);
  std::string DebugString() override;
};

class MapOperator : public Operator {
 public:
  explicit MapOperator(int64_t id) : Operator(id, planpb::MAP_OPERATOR) {}
  virtual ~MapOperator() {}

  Status Init(const planpb::MapOperator &pb);
  std::string DebugString() override;
};

class BlockingAggregateOperator : public Operator {
 public:
  explicit BlockingAggregateOperator(int64_t id)
      : Operator(id, planpb::BLOCKING_AGGREGATE_OPERATOR) {}
  virtual ~BlockingAggregateOperator() {}

  Status Init(const planpb::BlockingAggregateOperator &pb);
  std::string DebugString() override;
};

class MemorySinkOperator : public Operator {
 public:
  explicit MemorySinkOperator(int64_t id) : Operator(id, planpb::MEMORY_SINK_OPERATOR) {}
  virtual ~MemorySinkOperator() {}

  Status Init(const planpb::MemorySinkOperator &pb);
  std::string DebugString() override;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
