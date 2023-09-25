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
#include <utility>
#include <vector>

#include "src/carnot/planner/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/plannerpb/service.pb.h"
#include "src/carnot/planner/probes/label_selector_target.h"
#include "src/carnot/planner/probes/process_target.h"
#include "src/carnot/planner/probes/shared_object.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using TracepointSelector = carnot::planner::dynamic_tracing::ir::logical::TracepointSelector;

class ProbeOutput {
 public:
  ProbeOutput() = delete;
  ProbeOutput(const std::string& output_name, const std::vector<std::string>& col_names,
              const std::vector<std::string>& var_names)
      : output_name_(output_name), col_names_(col_names), var_names_(var_names) {}
  ProbeOutput(const std::vector<std::string>& col_names, const std::vector<std::string>& var_names)
      : col_names_(col_names), var_names_(var_names) {}
  /**
   * @brief Returns the schema as a string. Just joins column_names together. If two outputActions
   * write to the same columns w/ the same schemas, they can still disagree on type which will be
   * piped up in a later Stirling IR processing stage.
   *
   * @return std::string_view the string representation of the schema.
   */
  // std::string_view Schema() const { return absl::StrJoin(col_names_, ","); }

  Status ToActionProto(carnot::planner::dynamic_tracing::ir::logical::OutputAction* pb);
  Status ToOutputProto(carnot::planner::dynamic_tracing::ir::logical::Output* pb);
  const std::string& name() const { return output_name_; }

  void set_name(const std::string& output_name) { output_name_ = output_name; }

 private:
  // Output name is the name of the output to write.
  std::string output_name_;
  // Column names are the names to write to.
  std::vector<std::string> col_names_;
  // Var names are the ids of the variables to write to those columns.
  std::vector<std::string> var_names_;
};

class TracepointIR {
 public:
  explicit TracepointIR(const std::string& function_name) : symbol_(function_name) {}

  /**
   * @brief Serializes this probe definition as a protobuf.
   *
   * @param pb
   * @return Any error that occured during serialization.
   */
  Status ToProto(carnot::planner::dynamic_tracing::ir::logical::TracepointSpec* pb,
                 const std::string& name);

  /**
   * @return true a latency column has been defined for the probe
   * @return false otherwise
   */
  bool HasLatencyCol() { return latency_col_id_.size() > 0; }

  /**
   * @brief Set the ID of a function latency object. There can only be one, so this is structured
   * differently than arguments and return values.
   *
   * @param latency_col_id
   */
  void SetFunctionLatencyID(const std::string& latency_col_id) { latency_col_id_ = latency_col_id; }

  /**
   * @brief Add Argument expression to the probe.
   *
   * @param id the id of the argument expression.
   * @param expr the expression to parse for the argument.
   */
  void AddArgument(const std::string& id, const std::string& expr);

  /**
   * @brief Add Return expression to the probe.
   *
   * @param id the id of the argument expression.
   * @param expr the expression to parse for the argument.
   */
  void AddReturnValue(const std::string& id, const std::string& expr);

  /**
   * @brief Adds a latency function to this probe.
   *
   * @param id the id of the returned value.
   */
  void AddLatencyFunc(const std::string& id);

  /**
   * @brief Convenience function that returns the next string to set as an id of an argument. In the
   * future we might name these.
   *
   * @return std::string
   */
  std::string NextArgName() { return absl::Substitute("arg$0", args_.size()); }

  /**
   * @brief Convenience function that returns the next string to set as an id of return value. In
   * the future we might name these.
   *
   * @return std::string
   */
  std::string NextReturnName() { return absl::Substitute("ret$0", ret_vals_.size()); }

  /**
   * @brief Convenience function that returns the next string to set as an id of a latency function
   * call. We only support 1 latency call for an entire function so we always return the same value.
   *
   * @return std::string
   */
  std::string NextLatencyName() { return "lat0"; }

  /**
   * @brief Create a New Output definition with the given col_names and var_names. For now we only
   * support one output.
   *
   * @param col_names
   * @param var_names
   */
  void CreateNewOutput(const std::vector<std::string>& col_names,
                       const std::vector<std::string>& var_names);

  void SetOutputName(const std::string& output_name);

  std::shared_ptr<ProbeOutput> output() const { return output_; }

 private:
  std::string symbol_;
  std::string latency_col_id_;
  std::vector<carnot::planner::dynamic_tracing::ir::logical::Argument> args_;
  std::vector<carnot::planner::dynamic_tracing::ir::logical::ReturnValue> ret_vals_;
  std::shared_ptr<ProbeOutput> output_ = nullptr;
};

class TracepointDeployment {
 public:
  TracepointDeployment(const std::string& trace_name, int64_t ttl_ns)
      : name_(trace_name), ttl_ns_(ttl_ns) {}
  /**
   * @brief Converts Program to the proto representation.
   *
   * @param pb
   * @return Status errors if they happen.
   */
  Status ToProto(carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment* pb) const;

  /**
   * @brief Add a Probe to the current program being traced.
   *
   * @param probe_ir
   * @param probe_name
   * @param output_name the name of the touput table for the probe.
   * @return Status
   */
  Status AddTracepoint(TracepointIR* probe_ir, const std::string& probe_name,
                       const std::string& output_name);

  /**
   * @brief Sets the BPF trace program for a deployment.
   *
   * @param bpftrace_program the program in string format.
   * @param output_name the output table to write program results.
   * @param selectors the selectors to use for the program.
   * @return Status
   */
  Status AddBPFTrace(const std::string& bpftrace_str, const std::string& output_name,
                     const std::vector<TracepointSelector>& selectors);

  std::string name() const { return name_; }

 private:
  std::string name_;
  int64_t ttl_ns_;
  std::string binary_path_;
  std::vector<carnot::planner::dynamic_tracing::ir::logical::Probe> probes_;
  std::vector<
      carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment::TracepointProgram>
      tracepoints_;
  std::vector<carnot::planner::dynamic_tracing::ir::logical::Output> outputs_;
  absl::flat_hash_map<std::string, carnot::planner::dynamic_tracing::ir::logical::Output*>
      output_map_;
};

class MutationsIR {
 public:
  /**
   * @brief Creates a new probe definition and stores it in the current_probe() of the Builder.
   *
   * @param function_name
   * @return std::shared_ptr<TracepointIR>
   */
  std::shared_ptr<TracepointIR> StartProbe(const std::string& function_name);

  /**
   * @brief Create a TraceProgram for the MutationsIR w/ the specified UPID.
   *
   * @param program_name
   * @param upid
   * @param ttl_ns
   * @return StatusOr<TracepointDeployment*>
   */
  StatusOr<TracepointDeployment*> CreateTracepointDeployment(const std::string& tracepoint_name,
                                                             const md::UPID& upid, int64_t ttl_ns);

  /**
   * @brief Create a TraceProgram for the MutationsIR w/ the specified SharedObject.
   *
   * @param program_name
   * @param shared_obj
   * @param ttl_ns
   * @return StatusOr<TracepointDeployment*>
   */
  StatusOr<TracepointDeployment*> CreateTracepointDeployment(const std::string& tracepoint_name,
                                                             const SharedObject& shared_obj,
                                                             int64_t ttl_ns);

  /**
   * @brief Create a TraceProgram for the MutationsIR w/ the specified Pod name.
   *
   * @param program_name
   * @param pod_name
   * @param ttl_ns
   * @return StatusOr<TracepointDeployment*>
   */
  StatusOr<TracepointDeployment*> CreateTracepointDeploymentOnPod(
      const std::string& tracepoint_name, const std::string& pod_name, int64_t ttl_ns);
  /**
   * @brief Create a TraceProgram for the MutationsIR w/ the specified Pod name.
   *
   * @param program_name
   * @param pod_name
   * @param ttl_ns
   * @return StatusOr<TracepointDeployment*>
   */
  StatusOr<TracepointDeployment*> CreateTracepointDeploymentOnProcessSpec(
      const std::string& tracepoint_name, const ProcessSpec& pod_name, int64_t ttl_ns);

  /**
   * @brief Create a TraceProgram for the MutationsIR w/ the specified Pod name.
   *
   * @param tracepoint_name
   * @param spec
   * @param ttl_ns
   * @return StatusOr<TracepointDeployment*>
   */
  StatusOr<TracepointDeployment*> CreateTracepointDeploymentOnLabelSelectorSpec(
      const std::string& tracepoint_name, const LabelSelectorSpec& spec, int64_t ttl_ns);

  StatusOr<TracepointDeployment*> CreateKProbeTracepointDeployment(
      const std::string& tracepoint_name, int64_t ttl_ns);
  /**
   * @brief Get the CurrentProbe or return an error. Nice shorthand to support a clean error
   * message that points to the write position in the code.
   *
   * @param ast the ast object to use as a pointer to the line of code.
   * @return StatusOr<TracepointIR*> the current probe or an error if it doesn't exist.
   */
  StatusOr<TracepointIR*> GetCurrentProbeOrError(const pypa::AstPtr& ast);

  /**
   * @brief Converts the Probe definitions into the proto definition that can be used by the
   * Stirling probe tracer. Note: If there are multiple binaries defined then this will error out
   * for now. In the future we might support multiple binaries, but for now it will error out.
   *
   * @param pb the protobuf object to write to.
   * @return Status
   */
  Status ToProto(plannerpb::CompileMutationsResponse* pb);

  /**
   * @brief Stops recording changes to the current_tracepoint_ and removes it from the
   * current_tracepoint_ position.
   */
  void EndProbe();

  /**
   * @brief Deletes the tracepoint passed in.
   *
   * @param tracepoint_to_delete
   */
  void DeleteTracepoint(const std::string& tracepoint_to_delete) {
    tracepoints_to_delete_.push_back(tracepoint_to_delete);
  }

  const std::vector<std::string>& TracepointsToDelete() { return tracepoints_to_delete_; }

  TracepointIR* current_probe() { return current_tracepoint_.get(); }

  /**
   * @brief Add a config value for the particular asid.
   *
   * @param asid
   * @param key
   * @param value
   */
  void AddConfig(const std::string& pem_pod_name, const std::string& key, const std::string& value);

  std::vector<TracepointDeployment*> Deployments();

 private:
  // All the new tracepoints added as part of this mutation. DeploymentSpecs are protobufs because
  // we only modify these upon inserting the new tracepoint, while the Tracepoint definition is
  // still modified aftered adding the tracepoint.
  std::vector<std::pair<carnot::planner::dynamic_tracing::ir::logical::DeploymentSpec,
                        std::unique_ptr<TracepointDeployment>>>
      deployments_;

  std::vector<std::unique_ptr<TracepointDeployment>> bpftrace_programs_;

  std::vector<std::shared_ptr<TracepointIR>> probes_pool_;
  std::shared_ptr<TracepointIR> current_tracepoint_;

  std::vector<std::string> tracepoints_to_delete_;

  // The updates to internal config that need to be done.
  std::vector<plannerpb::ConfigUpdate> config_updates_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
