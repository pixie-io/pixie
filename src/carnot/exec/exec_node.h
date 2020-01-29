#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

enum class ExecNodeType : int8_t {
  kSourceNode = 0,
  kSinkNode = 1,
  kProcessingNode = 2,
};

/**
 * This is the base class for the execution nodes in Carnot.
 */
class ExecNode {
 public:
  ExecNode() = delete;
  virtual ~ExecNode() = default;

  /**
   * Init is called with plan & schema information.
   * @param plan_node the plan class of the node.
   * @param output_descriptor The output column schema of row batches.
   * @param input_descriptors The input column schema of row batches.
   * @return
   */
  Status Init(const plan::Operator& plan_node,
              const table_store::schema::RowDescriptor& output_descriptor,
              std::vector<table_store::schema::RowDescriptor> input_descriptors) {
    output_descriptor_ = std::make_unique<table_store::schema::RowDescriptor>(output_descriptor);
    input_descriptors_ = input_descriptors;
    return InitImpl(plan_node);
  }

  /**
   * Setup internal data structures, perform validation, etc.
   * @param exec_state The execution state.
   * @return The status of the prepare.
   */
  Status Prepare(ExecState* exec_state) { return PrepareImpl(exec_state); }

  /**
   * Acquire memory resources, etc.
   * @param exec_state The execution state.
   * @return
   */
  Status Open(ExecState* exec_state) { return OpenImpl(exec_state); }

  /**
   * Close is where cleanup should take place. This includes cleaning up objects.
   * It is highly recomended that a default destructor be used and cleanup peformed here,
   * since at the end a query the data is batch deleted and ordering is not guaranteed.
   * @param exec_state The execution state.
   * @return The status of the Finalize.axs
   */
  Status Close(ExecState* exec_state) { return CloseImpl(exec_state); }

  /**
   * GenerateNext is called to produce the next row batch. This is only valid
   * on source nodes (and will result in an error on other nodes).
   * @param exec_state The execution state.
   * @return The status of the execution.
   */
  Status GenerateNext(ExecState* exec_state) {
    DCHECK(type() == ExecNodeType::kSourceNode);
    return GenerateNextImpl(exec_state);
  }

  /**
   * Consume the next row batch. This function is only valid for Sink and Processing
   * Nodes.
   *
   * This needs to be careful to forward the output batch to all children.
   *
   * @param exec_state The execution state.
   * @param rb The input row batch.
   * @return The Status of consumption.
   */
  Status ConsumeNext(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                     size_t parent_index) {
    DCHECK(type() == ExecNodeType::kSinkNode || type() == ExecNodeType::kProcessingNode);

    if (rb.eos() && !rb.eow()) {
      return error::Internal(
          "ConsumeNext received row batch with end of stream set but not end of window.");
    }
    return ConsumeNextImpl(exec_state, rb, parent_index);
  }

  /**
   * Check if it's a source node.
   */
  bool IsSource() { return type() == ExecNodeType::kSourceNode; }

  /**
   * Check if it's a sink node.
   */
  bool IsSink() { return type() == ExecNodeType::kSinkNode; }

  /**
   * Check if it's a processing node.
   */
  bool IsProcessing() { return type() == ExecNodeType::kProcessingNode; }

  /**
   * Get a debug string for the node.
   * @return the debug string/
   */
  std::string DebugString() { return DebugStringImpl(); }

  /**
   * Add a new child node where data is forwarded.
   * This node will not own the child. The lifetime of the child should
   * exceed the lifetime of this node.
   * The node also needs to know which parent index it is for its child.
   * A node that is the 2nd parent of a child needs to pass that information
   * down when it sends that child row batches so the child can differentiate
   * between the row batches of its various parents.
   *
   * @param child Another execution node.
   */
  void AddChild(ExecNode* child, size_t parent_index) {
    children_.emplace_back(child);
    parent_ids_for_children_.emplace_back(parent_index);
  }

  /**
   * Get the type of the execution node.
   * @return the ExecNodeType.
   */
  ExecNodeType type() { return type_; }

  /**
   * @ return the children of the execution node.
   */
  std::vector<ExecNode*> children() { return children_; }

 protected:
  /**
   * Send data to children row batches.
   * @param exec_state The exec state.
   * @param rb The row batch to send.
   * @return Status of children execution.
   */
  Status SendRowBatchToChildren(ExecState* exec_state, const table_store::schema::RowBatch& rb) {
    for (size_t i = 0; i < children_.size(); ++i) {
      PL_RETURN_IF_ERROR(children_[i]->ConsumeNext(exec_state, rb, parent_ids_for_children_[i]));
    }
    if (rb.eos()) {
      DCHECK(!sent_eos_);
      sent_eos_ = true;
    }
    return Status::OK();
  }

  explicit ExecNode(ExecNodeType type) : type_(type) {}

  // Defines the protected implementations of the non-virtual interface functions
  // defined above.
  virtual std::string DebugStringImpl() = 0;
  virtual Status InitImpl(const plan::Operator& plan_node) = 0;
  virtual Status PrepareImpl(ExecState* exec_state) = 0;
  virtual Status OpenImpl(ExecState* exec_state) = 0;
  virtual Status CloseImpl(ExecState* exec_state) = 0;

  virtual Status GenerateNextImpl(ExecState*) {
    return error::Unimplemented("Implement in derived class (if source)");
  }

  virtual Status ConsumeNextImpl(ExecState*, const table_store::schema::RowBatch&, size_t) {
    return error::Unimplemented("Implement in derived class (if sink or processing)");
  }

  bool is_closed() { return is_closed_; }

  std::unique_ptr<table_store::schema::RowDescriptor> output_descriptor_;
  std::vector<table_store::schema::RowDescriptor> input_descriptors_;
  // Whether or not the node sent EOS to its children.
  bool sent_eos_ = false;

 private:
  // Unowned reference to the children. Must remain valid for the duration of query.
  std::vector<ExecNode*> children_;
  // For each of the children (which may have multiple parents) which parent is this node?
  // Parents 0, 1, and 2 would exist for a node with 3 parents.
  std::vector<size_t> parent_ids_for_children_;
  bool is_closed_ = false;
  // The type of execution node.
  ExecNodeType type_;
};

/**
 * Processing node is the base class for anything that computes
 * producing 1:1 or N:M records. For example: Agg, Map, etc.
 */
class ProcessingNode : public ExecNode {
 public:
  ProcessingNode() : ExecNode(ExecNodeType::kProcessingNode) {}
};

/**
 * Source node is the base class for anything that produces records from some source.
 * For example: MemorySource.
 */
class SourceNode : public ExecNode {
 public:
  SourceNode() : ExecNode(ExecNodeType::kSourceNode) {}
  virtual bool HasBatchesRemaining() { return !sent_eos_; }
  virtual bool NextBatchReady() = 0;
  int64_t BytesProcessed() const { return bytes_processed_; }
  int64_t RowsProcessed() const { return rows_processed_; }

 protected:
  int64_t rows_processed_ = 0;
  int64_t bytes_processed_ = 0;
};

/**
 * Sink node is the base class for anything that consumes records and writes to some sink.
 * For example: MemorySink.
 */
class SinkNode : public ExecNode {
 public:
  SinkNode() : ExecNode(ExecNodeType::kSinkNode) {}
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
