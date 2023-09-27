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

#include "src/carnot/planner/probes/probes.h"

#include <utility>

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

Status TracepointIR::ToProto(carnot::planner::dynamic_tracing::ir::logical::TracepointSpec* pb,
                             const std::string& probe_name) {
  auto* probe_pb = pb->mutable_probe();
  probe_pb->set_name(probe_name);
  auto* tracepoint_pb = probe_pb->mutable_tracepoint();
  tracepoint_pb->set_symbol(symbol_);

  for (const auto& arg : args_) {
    *probe_pb->add_args() = arg;
  }

  for (const auto& retval : ret_vals_) {
    *probe_pb->add_ret_vals() = retval;
  }

  if (HasLatencyCol()) {
    probe_pb->mutable_function_latency()->set_id(latency_col_id_);
  }

  // Probes don't necessarily have an output. IE if our probe just writes to a map.
  if (output_) {
    PX_RETURN_IF_ERROR(output_->ToActionProto(probe_pb->add_output_actions()));
    PX_RETURN_IF_ERROR(output_->ToOutputProto(pb->add_outputs()));
  }
  return Status::OK();
}

Status ProbeOutput::ToActionProto(carnot::planner::dynamic_tracing::ir::logical::OutputAction* pb) {
  pb->set_output_name(output_name_);
  for (const auto& var : var_names_) {
    pb->add_variable_names(var);
  }
  return Status::OK();
}

Status ProbeOutput::ToOutputProto(carnot::planner::dynamic_tracing::ir::logical::Output* pb) {
  pb->set_name(output_name_);
  for (const auto& col : col_names_) {
    pb->add_fields(col);
  }
  return Status::OK();
}

void TracepointIR::SetOutputName(const std::string& output_name) {
  if (!output_) {
    return;
  }
  output_->set_name(output_name);
}

void TracepointIR::CreateNewOutput(const std::vector<std::string>& col_names,
                                   const std::vector<std::string>& var_names) {
  output_ = std::make_shared<ProbeOutput>(col_names, var_names);
}

void TracepointIR::AddArgument(const std::string& id, const std::string& expr) {
  carnot::planner::dynamic_tracing::ir::logical::Argument arg;
  arg.set_id(id);
  arg.set_expr(expr);
  args_.push_back(arg);
}

void TracepointIR::AddReturnValue(const std::string& id, const std::string& expr) {
  carnot::planner::dynamic_tracing::ir::logical::ReturnValue ret;
  ret.set_id(id);
  // TODO(philkuz/oazizi) The expression needs to be in the form "$<index>.<field>.<...>".
  ret.set_expr(expr);
  ret_vals_.push_back(ret);
}

std::vector<TracepointDeployment*> MutationsIR::Deployments() {
  std::vector<TracepointDeployment*> deployments;
  for (size_t i = 0; i < deployments_.size(); i++) {
    deployments.push_back(deployments_[i].second.get());
  }

  for (size_t i = 0; i < bpftrace_programs_.size(); i++) {
    deployments.push_back(bpftrace_programs_[i].get());
  }

  return deployments;
}

std::shared_ptr<TracepointIR> MutationsIR::StartProbe(const std::string& function_name) {
  auto tracepoint_ir = std::make_shared<TracepointIR>(function_name);
  probes_pool_.push_back(tracepoint_ir);
  current_tracepoint_ = tracepoint_ir;
  return tracepoint_ir;
}

StatusOr<TracepointDeployment*> MutationsIR::CreateTracepointDeployment(
    const std::string& tracepoint_name, const md::UPID& upid, int64_t ttl_ns) {
  std::unique_ptr<TracepointDeployment> program =
      std::make_unique<TracepointDeployment>(tracepoint_name, ttl_ns);
  TracepointDeployment* raw = program.get();

  carnot::planner::dynamic_tracing::ir::logical::DeploymentSpec deployment_spec;
  auto upid_pb = deployment_spec.mutable_upid();
  upid_pb->set_asid(upid.asid());
  upid_pb->set_pid(upid.pid());
  upid_pb->set_ts_ns(upid.start_ts());

  deployments_.emplace_back(deployment_spec, std::move(program));
  return raw;
}

StatusOr<TracepointDeployment*> MutationsIR::CreateTracepointDeployment(
    const std::string& tracepoint_name, const SharedObject& shared_object, int64_t ttl_ns) {
  std::unique_ptr<TracepointDeployment> program =
      std::make_unique<TracepointDeployment>(tracepoint_name, ttl_ns);
  TracepointDeployment* raw = program.get();

  carnot::planner::dynamic_tracing::ir::logical::DeploymentSpec deployment_spec;
  auto shared_object_pb = deployment_spec.mutable_shared_object();

  shared_object_pb->set_name(shared_object.name());

  auto upid_pb = shared_object_pb->mutable_upid();
  upid_pb->set_asid(shared_object.upid().asid());
  upid_pb->set_pid(shared_object.upid().pid());
  upid_pb->set_ts_ns(shared_object.upid().start_ts());
  deployments_.emplace_back(deployment_spec, std::move(program));

  return raw;
}

StatusOr<TracepointDeployment*> MutationsIR::CreateTracepointDeploymentOnProcessSpec(
    const std::string& tracepoint_name, const ProcessSpec& process_spec, int64_t ttl_ns) {
  std::unique_ptr<TracepointDeployment> program =
      std::make_unique<TracepointDeployment>(tracepoint_name, ttl_ns);
  TracepointDeployment* raw = program.get();

  carnot::planner::dynamic_tracing::ir::logical::DeploymentSpec deployment_spec;
  auto pod_process = deployment_spec.mutable_pod_process();
  pod_process->add_pods(process_spec.pod_name_);
  pod_process->set_container(process_spec.container_name_);
  pod_process->set_process(process_spec.process_);
  deployments_.emplace_back(deployment_spec, std::move(program));
  return raw;
}

StatusOr<TracepointDeployment*> MutationsIR::CreateTracepointDeploymentOnLabelSelectorSpec(
    const std::string& tracepoint_name, const LabelSelectorSpec& label_selector_spec,
    int64_t ttl_ns) {
  std::unique_ptr<TracepointDeployment> program =
      std::make_unique<TracepointDeployment>(tracepoint_name, ttl_ns);
  TracepointDeployment* raw = program.get();

  carnot::planner::dynamic_tracing::ir::logical::DeploymentSpec deployment_spec;
  auto label_selector = deployment_spec.mutable_label_selector();
  auto match_labels = label_selector->mutable_labels();
  for (auto& label : label_selector_spec.labels_) {
    (*match_labels)[label.first] = label.second;
  }

  label_selector->set_namespace_(label_selector_spec.namespace_);
  label_selector->set_container(label_selector_spec.container_name_);
  label_selector->set_process(label_selector_spec.process_);
  deployments_.emplace_back(deployment_spec, std::move(program));
  return raw;
}

StatusOr<TracepointDeployment*> MutationsIR::CreateKProbeTracepointDeployment(
    const std::string& tracepoint_name, int64_t ttl_ns) {
  std::unique_ptr<TracepointDeployment> program =
      std::make_unique<TracepointDeployment>(tracepoint_name, ttl_ns);
  TracepointDeployment* raw = program.get();

  bpftrace_programs_.push_back(std::move(program));
  return raw;
}

Status TracepointDeployment::AddBPFTrace(const std::string& bpftrace_str,
                                         const std::string& output_name,
                                         const std::vector<TracepointSelector>& selectors) {
  carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment::TracepointProgram
      tracepoint_pb;
  tracepoint_pb.mutable_bpftrace()->set_program(bpftrace_str);
  // set the output table to write program results to
  tracepoint_pb.set_table_name(output_name);
  for (const auto& selector : selectors) {
    *tracepoint_pb.add_selectors() = selector;
  }
  tracepoints_.push_back(tracepoint_pb);
  return Status::OK();
}

Status TracepointDeployment::AddTracepoint(TracepointIR* tracepoint_ir,
                                           const std::string& probe_name,
                                           const std::string& output_name) {
  tracepoint_ir->SetOutputName(output_name);

  carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment::TracepointProgram
      tracepoint_pb;
  PX_CHECK_OK(tracepoint_ir->ToProto(tracepoint_pb.mutable_spec(), probe_name));
  tracepoint_pb.set_table_name(output_name);
  tracepoints_.push_back(tracepoint_pb);

  auto output = tracepoint_ir->output();
  if (!output) {
    return Status::OK();
  }
  // Upsert the output definition.
  carnot::planner::dynamic_tracing::ir::logical::Output output_pb;
  PX_RETURN_IF_ERROR(output->ToOutputProto(&output_pb));

  // If the output name is missing, then we need to add it.
  if (!output_map_.contains(output->name())) {
    outputs_.push_back(output_pb);
    output_map_[output->name()] = &output_pb;
    return Status::OK();
  }
  // Otherwise, we make sure the output schema here matches the already added schema.
  const auto& new_schema = output_pb.DebugString();
  const auto& old_schema = output_map_[output->name()]->DebugString();
  if (old_schema != new_schema) {
    return error::InvalidArgument(
        "New output schema '$0' for '$2' doesnt match previously defined schema '$1'", new_schema,
        old_schema, output->name());
  }
  return Status::OK();
}

StatusOr<TracepointIR*> MutationsIR::GetCurrentProbeOrError(const pypa::AstPtr& ast) {
  if (current_tracepoint_.get() == nullptr) {
    return CreateAstError(ast, "Missing current probe");
  }
  return current_tracepoint_.get();
}

Status TracepointDeployment::ToProto(
    carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment* pb) const {
  for (const auto& tracepoint : tracepoints_) {
    (*pb->add_programs()) = tracepoint;
  }
  pb->set_name(name_);

  auto one_sec = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
  pb->mutable_ttl()->set_seconds(ttl_ns_ / one_sec.count());
  pb->mutable_ttl()->set_nanos(ttl_ns_ % one_sec.count());
  return Status::OK();
}
void MutationsIR::AddConfig(const std::string& pem_pod_name, const std::string& key,
                            const std::string& value) {
  plannerpb::ConfigUpdate update;
  update.set_key(key);
  update.set_value(value);
  update.set_agent_pod_name(pem_pod_name);
  config_updates_.push_back(update);
}

Status MutationsIR::ToProto(plannerpb::CompileMutationsResponse* pb) {
  for (const auto& [spec, program] : deployments_) {
    auto program_pb = pb->add_mutations()->mutable_trace();
    PX_RETURN_IF_ERROR(program->ToProto(program_pb));
    *(program_pb->mutable_deployment_spec()) = spec;
  }

  for (const auto& program : bpftrace_programs_) {
    auto program_pb = pb->add_mutations()->mutable_trace();
    PX_RETURN_IF_ERROR(program->ToProto(program_pb));
  }

  for (const auto& tracepoint_to_delete : TracepointsToDelete()) {
    pb->add_mutations()->mutable_delete_tracepoint()->set_name(tracepoint_to_delete);
  }

  for (const auto& update : config_updates_) {
    *(pb->add_mutations()->mutable_config_update()) = update;
  }

  return Status::OK();
}

void MutationsIR::EndProbe() { current_tracepoint_ = nullptr; }

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
