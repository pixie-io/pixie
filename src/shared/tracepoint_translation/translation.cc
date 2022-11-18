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

#include "src/shared/tracepoint_translation/translation.h"

namespace px {
namespace tracepoint {

void CopyUPID(const carnot::planner::dynamic_tracing::ir::logical::UPID& in,
              stirling::dynamic_tracing::ir::shared::UPID* out) {
  out->set_asid(in.asid());
  out->set_ts_ns(in.ts_ns());
  out->set_pid(in.pid());
}

void CopyDeploymentSpec(const carnot::planner::dynamic_tracing::ir::logical::DeploymentSpec& in,
                        stirling::dynamic_tracing::ir::shared::DeploymentSpec* out) {
  if (in.has_upid()) {
    CopyUPID(in.upid(), out->mutable_upid_list()->add_upids());
    return;
  } else if (in.has_shared_object()) {
    auto shared_object = out->mutable_shared_object();
    shared_object->set_name(in.shared_object().name());
    CopyUPID(in.shared_object().upid(), shared_object->mutable_upid());
    return;
  } else if (in.has_pod_process()) {
    auto pod_process = out->mutable_pod_process();
    for (const auto& pod : in.pod_process().pods()) {
      pod_process->add_pods(pod);
    }
    pod_process->set_process(in.pod_process().process());
    pod_process->set_container(in.pod_process().container());
    return;
  }
}

void CopyOutput(const carnot::planner::dynamic_tracing::ir::logical::Output& in,
                stirling::dynamic_tracing::ir::logical::Output* out) {
  out->set_name(in.name());
  for (const auto& field : in.fields()) {
    out->add_fields(field);
  }
}

void CopyArg(const carnot::planner::dynamic_tracing::ir::logical::Argument& in,
             stirling::dynamic_tracing::ir::logical::Argument* out) {
  out->set_id(in.id());
  out->set_expr(in.expr());
}

void CopyReturnValue(const carnot::planner::dynamic_tracing::ir::logical::ReturnValue& in,
                     stirling::dynamic_tracing::ir::logical::ReturnValue* out) {
  out->set_id(in.id());
  out->set_expr(in.expr());
}

void CopyOutputActions(const carnot::planner::dynamic_tracing::ir::logical::OutputAction& in,
                       stirling::dynamic_tracing::ir::logical::OutputAction* out) {
  out->set_output_name(in.output_name());
  for (const auto& in_var : in.variable_names()) {
    out->add_variable_names(in_var);
  }
}

void CopyTracepoint(const carnot::planner::dynamic_tracing::ir::logical::Tracepoint& in,
                    stirling::dynamic_tracing::ir::shared::Tracepoint* out) {
  out->set_symbol(in.symbol());
  // We always set the type to Logical.
  out->set_type(
      ::px::stirling::dynamic_tracing::ir::shared::Tracepoint_Type::Tracepoint_Type_LOGICAL);
}

void CopyProbe(const carnot::planner::dynamic_tracing::ir::logical::Probe& in,
               stirling::dynamic_tracing::ir::logical::Probe* out) {
  out->set_name(in.name());

  if (in.has_tracepoint()) {
    CopyTracepoint(in.tracepoint(), out->mutable_tracepoint());
  }

  for (const auto& arg : in.args()) {
    CopyArg(arg, out->add_args());
  }
  for (const auto& ret_val : in.ret_vals()) {
    CopyReturnValue(ret_val, out->add_ret_vals());
  }

  if (in.has_function_latency()) {
    out->mutable_function_latency()->set_id(in.function_latency().id());
  }

  for (const auto& output_action : in.output_actions()) {
    CopyOutputActions(output_action, out->add_output_actions());
  }
}

void CopyTracepointSpec(const carnot::planner::dynamic_tracing::ir::logical::TracepointSpec& in,
                        stirling::dynamic_tracing::ir::logical::TracepointSpec* out) {
  for (const auto& output : in.outputs()) {
    CopyOutput(output, out->add_outputs());
  }
  if (in.has_probe()) {
    CopyProbe(in.probe(), out->add_probes());
  }
}

void CopyBPFTrace(const carnot::planner::dynamic_tracing::ir::logical::BPFTrace& in,
                  stirling::dynamic_tracing::ir::logical::BPFTrace* out) {
  out->set_program((in.program()));
}

void CopyTracepointProgram(
    const carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment::TracepointProgram&
        in,
    stirling::dynamic_tracing::ir::logical::TracepointDeployment::Tracepoint* out) {
  out->set_table_name(in.table_name());
  // "spec" field is named "program" in stirling IR.
  if (in.has_spec()) {
    CopyTracepointSpec(in.spec(), out->mutable_program());
  }
  if (in.has_bpftrace()) {
    CopyBPFTrace(in.bpftrace(), out->mutable_bpftrace());
  }
}

void ConvertPlannerTracepointToStirlingTracepoint(
    const carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment& in,
    stirling::dynamic_tracing::ir::logical::TracepointDeployment* out) {
  out->set_name(in.name());
  if (in.has_ttl()) {
    (*out->mutable_ttl()) = in.ttl();
  }
  if (in.has_deployment_spec()) {
    CopyDeploymentSpec(in.deployment_spec(), out->mutable_deployment_spec());
  }

  for (const auto& in_program : in.programs()) {
    // "programs" field is named "tracepoints" in stirling IR.
    CopyTracepointProgram(in_program, out->add_tracepoints());
  }
}

}  // namespace tracepoint
}  // namespace px
