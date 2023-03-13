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
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief IR for the network sink operator that passes batches over GRPC to the destination.
 *
 * Setting up the Sink Operator requires three steps.
 * 0. Init(int destination_id): Set the destination id.
 * 1. SetDistributedID(string): Set the name of the node same as the query broker.
 * 2. SetDestinationAddress(string): the GRPC address where batches should be sent.
 */
class GRPCSinkIR : public OperatorIR {
 public:
  explicit GRPCSinkIR(int64_t id) : OperatorIR(id, IRNodeType::kGRPCSink) {}

  enum GRPCSinkType {
    kTypeNotSet = 0,
    kInternal,
    kExternal,
  };

  // Init function to call to create an internal GRPCSink, which sends an intermediate
  // result to a corresponding GRPC Source.
  Status Init(OperatorIR* parent, int64_t destination_id) {
    PX_RETURN_IF_ERROR(AddParent(parent));
    destination_id_ = destination_id;
    sink_type_ = GRPCSinkType::kInternal;
    return Status::OK();
  }

  // Init function to call to create an external, final result producing GRPCSink, which
  // streams the output table to a non-Carnot destination (such as the query broker).
  Status Init(OperatorIR* parent, const std::string& name,
              const std::vector<std::string> out_columns) {
    PX_RETURN_IF_ERROR(AddParent(parent));
    sink_type_ = GRPCSinkType::kExternal;
    name_ = name;
    out_columns_ = out_columns;
    return Status::OK();
  }

  /**
   * @brief ToProto for GRPCSinks that send tables over.
   *
   * @param op_pb
   * @return Status
   */
  Status ToProto(planpb::Operator* op_pb) const override;

  /**
   * @brief ToProto for GRPCSinks that have destination ids. Support mostly-duplicate PEM plans that
   * differ solely on the destination IDs of the GRPCSinks.
   *
   * @param op_pb
   * @param destination_id the ID of GRPCSource node that this GRPCSink sends data to.
   * @return Status
   */
  Status ToProto(planpb::Operator* op_pb, int64_t destination_id) const;

  /**
   * @brief The id used for initial mapping. This associates a GRPCSink with a subsequent
   * GRPCSourceGroup.
   *
   * Once the Distributed Plan is established, you should use DistributedDestinationID().
   */
  bool has_destination_id() const { return sink_type_ == GRPCSinkType::kInternal; }
  int64_t destination_id() const { return destination_id_; }
  void SetDestinationID(int64_t destination_id) { destination_id_ = destination_id; }
  void AddDestinationIDMap(int64_t destination_id, int64_t agent_id) {
    agent_id_to_destination_id_[agent_id] = destination_id;
  }

  void SetDestinationAddress(const std::string& address) { destination_address_ = address; }
  // If needed, specify the ssl target name override for the GRPC sink destination.
  void SetDestinationSSLTargetName(std::string_view ssl_targetname) {
    destination_ssl_targetname_ = ssl_targetname;
  }

  const std::string& destination_address() const { return destination_address_; }
  bool DestinationAddressSet() const { return destination_address_ != ""; }
  const std::string& destination_ssl_targetname() const { return destination_ssl_targetname_; }

  bool has_output_table() const { return sink_type_ == GRPCSinkType::kExternal; }
  std::string name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }
  // When out_columns_ is empty, the full input relation will be written to the sink.
  const std::vector<std::string>& out_columns() const { return out_columns_; }

  inline bool IsBlocking() const override { return true; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    if (sink_type_ != GRPCSinkType::kExternal) {
      return error::Unimplemented("Unexpected call to GRPCSinkIR::RequiredInputColumns");
    }
    DCHECK(is_type_resolved());
    auto out_cols = resolved_table_type()->ColumnNames();
    absl::flat_hash_set<std::string> outputs{out_cols.begin(), out_cols.end()};
    return std::vector<absl::flat_hash_set<std::string>>{outputs};
  }

  Status ResolveType(CompilerState* compiler_state);

  const absl::flat_hash_map<int64_t, int64_t>& agent_id_to_destination_id() {
    return agent_id_to_destination_id_;
  }

 protected:
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to GRPCSinkIR::PruneOutputColumnsTo.");
  }

 private:
  std::string destination_address_ = "";
  std::string destination_ssl_targetname_ = "";
  GRPCSinkType sink_type_ = GRPCSinkType::kTypeNotSet;
  // Used when GRPCSinkType = kInternal.
  int64_t destination_id_ = -1;
  // Used when GRPCSinkType = kExternal.
  std::string name_;
  std::vector<std::string> out_columns_;
  absl::flat_hash_map<int64_t, int64_t> agent_id_to_destination_id_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
