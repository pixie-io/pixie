#pragma once
#include <memory>
#include <string>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

class MetadataObject : public QLObject {
 public:
  static constexpr TypeDescriptor MetadataTypeDescriptor = {
      /* name */ "metadata",
      /* type */ QLObjectType::kMetadata,
  };
  static StatusOr<std::shared_ptr<MetadataObject>> Create(OperatorIR* op, ASTVisitor* ast_visitor);
  inline static constexpr char kSubscriptHandler[] = R"doc(
  Creates the specified metadata as a column.

  Shorthand for exposing metadata columns in the DataFrame. Each metadata type
  can be converted from some sets of other metadata types that already exist in the DataFrame.

  Attempting to convert to a metadata type that doesn't have the source column will raise a compiler
  error.

  Available keys (and any aliases) as well as the source columns.
  * container_id (): Sources: "upid"
  * service_id: Sources: "upid","service_name"
  * pod_id: Sources: "upid","pod_name"
  * deployment_id: Sources: "upid","deployment_name"
  * service_name ("service"): Sources: "upid","service_id"
  * pod_name ("pod"): Sources: "upid","pod_id"
  * deployment_name ("deployment"): Sources: "upid","deployment_id"
  * namespace: Sources: "upid"
  * node_name ("node"): Sources: "upid"
  * hostname ("host"): Sources: "upid"
  * container_name ("container"): Sources: "upid"
  * cmdline ("cmd"): Sources: "upid"
  * asid: Sources: "upid"
  * pid: Sources: "upid"

  Args:
    metadata (string): The metadata property you wish to access. Function will throw an error if it doesn't exist.

  Returns:
    px.Column: Column that represents the metadata in the DataFrame. Can be used in a DataFrame expression.
  )doc";

 protected:
  explicit MetadataObject(OperatorIR* op, ASTVisitor* ast_visitor)
      : QLObject(MetadataTypeDescriptor, ast_visitor), op_(op) {}

  Status Init();

  StatusOr<QLObjectPtr> SubscriptHandler(const pypa::AstPtr& ast, const ParsedArgs& args);

  OperatorIR* op() const { return op_; }

 private:
  OperatorIR* op_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
