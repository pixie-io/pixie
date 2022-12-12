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
#include <memory>
#include <string>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class MetadataObject : public QLObject {
 public:
  static constexpr TypeDescriptor MetadataTypeDescriptor = {
      /* name */ "ctx",
      /* type */ QLObjectType::kMetadata,
  };
  static StatusOr<std::shared_ptr<MetadataObject>> Create(OperatorIR* op, ASTVisitor* ast_visitor);
  inline static constexpr char kCtxDocstring[] = R"doc(
  Creates the specified metadata as a column.

  Shorthand for exposing metadata columns in the DataFrame. Each metadata type
  can be converted from some sets of other metadata types that already exist in the DataFrame.

  Attempting to convert to a metadata type that doesn't have the source column will raise a compiler
  error.

  Available keys (and any aliases) as well as the source columns.
  * container_id (): Sources: "upid"
  * service_id: Sources: "upid","service_name"
  * service_name ("service"): Sources: "upid","service_id"
  * pod_id: Sources: "upid","pod_name"
  * pod_name ("pod"): Sources: "upid","pod_id"
  * deployment_id: Sources: "deployment_name","pod_id","pod_name","replicaset_name", "replicaset_id"
  * deployment_name ("deployment"): Sources: "upid","deployment_id","pod_id","pod_name","replicaset_name", "replicaset_id"
  * replicaset_id ("replica_set_id"): Sources: "upid","pod_id","pod_name", "replicaset_name"
  * replicaset_name ("replica_set", "replicaset"): Sources: "upid","pod_id","pod_name", "replicaset_id"
  * namespace: Sources: "upid"
  * node_name ("node"): Sources: "upid"
  * hostname ("host"): Sources: "upid"
  * container_name ("container"): Sources: "upid"
  * cmdline ("cmd"): Sources: "upid"
  * asid: Sources: "upid"
  * pid: Sources: "upid"

  :topic: dataframe_ops
  :opname: Metadata

  Examples:
    df = px.DataFrame('http_events', start_time='-5m')
    # Filter only for data that matches the metadata service.
    # df.ctx['service'] pulls the service column into the DataFrame.
    df = df[df.ctx['service'] == "pl/vizier-metadata"]
  Examples:
    df = px.DataFrame('http_events', start_time='-5m')
    # Add the service column from the metadata object.
    df.service = df.ctx['service']
    # Group by the service.
    df = df.groupby('service').agg(req_count=('service', px.count))
  Examples:
    # Where metadata context can fail.
    df = px.DataFrame('http_events', start_time='-5m')
    # Dropping upid so we remove the "source" column we could use
    df = df.drop('upid')
    # FAILS: no source column available for the metadata conversion.
    df.service = df.ctx['service']


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
}  // namespace px
