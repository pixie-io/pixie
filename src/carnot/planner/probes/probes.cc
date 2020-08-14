#include "src/carnot/planner/probes/probes.h"

#include <utility>

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

Status TracepointIR::ToProto(stirling::dynamic_tracing::ir::logical::TracepointSpec* pb,
                             const std::string& probe_name) {
  auto* probe_pb = pb->add_probes();
  probe_pb->set_name(probe_name);
  auto* tracepoint_pb = probe_pb->mutable_tracepoint();
  tracepoint_pb->set_symbol(symbol_);
  tracepoint_pb->set_type(stirling::dynamic_tracing::ir::shared::Tracepoint::LOGICAL);

  for (const auto& arg : args_) {
    *probe_pb->add_args() = arg;
  }

  for (const auto& retval : ret_vals_) {
    *probe_pb->add_ret_vals() = retval;
  }

  if (HasLatencyCol()) {
    probe_pb->mutable_function_latency()->set_id(latency_col_id_);
  }

  pb->set_language(language_);

  // TODO(philkuz) implement map methods.
  // for (const auto& map_stash : map_stash_actions_) {
  // (*probe_pb->add_map_stash_actions()) = map_stash;
  // }

  // Probes don't necessarily have an output. IE if our probe just writes to a map.
  if (output_) {
    PL_RETURN_IF_ERROR(output_->ToActionProto(probe_pb->add_output_actions()));
    PL_RETURN_IF_ERROR(output_->ToOutputProto(pb->add_outputs()));
  }
  return Status::OK();
}

Status ProbeOutput::ToActionProto(stirling::dynamic_tracing::ir::logical::OutputAction* pb) {
  pb->set_output_name(output_name_);
  for (const auto& var : var_names_) {
    pb->add_variable_name(var);
  }
  return Status::OK();
}

Status ProbeOutput::ToOutputProto(stirling::dynamic_tracing::ir::logical::Output* pb) {
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
  stirling::dynamic_tracing::ir::logical::Argument arg;
  arg.set_id(id);
  arg.set_expr(expr);
  args_.push_back(arg);
}

void TracepointIR::AddReturnValue(const std::string& id, const std::string& expr) {
  stirling::dynamic_tracing::ir::logical::ReturnValue ret;
  ret.set_id(id);
  // TODO(philkuz/oazizi) The expression needs to be in the form "$<index>.<field>.<...>".
  ret.set_expr(expr);
  ret_vals_.push_back(ret);
}

std::shared_ptr<TracepointIR> MutationsIR::StartProbe(
    stirling::dynamic_tracing::ir::shared::Language language, const std::string& function_name) {
  auto tracepoint_ir = std::make_shared<TracepointIR>(language, function_name);
  probes_pool_.push_back(tracepoint_ir);
  current_tracepoint_ = tracepoint_ir;
  return tracepoint_ir;
}

StatusOr<TracepointDeployment*> MutationsIR::CreateTracepointDeployment(
    const std::string& tracepoint_name, const md::UPID& upid, int64_t ttl_ns) {
  if (!upid_to_program_map_.empty() && upid_to_program_map_.contains(upid)) {
    return error::InvalidArgument(
        "Cannot UpsertTracepoint on the same binary. Use UpsertTracepoints instead.");
  }
  std::unique_ptr<TracepointDeployment> program =
      std::make_unique<TracepointDeployment>(tracepoint_name, ttl_ns);
  TracepointDeployment* raw = program.get();
  upid_to_program_map_[upid] = std::move(program);
  return raw;
}

Status TracepointDeployment::AddTracepoint(TracepointIR* tracepoint_ir,
                                           const std::string& probe_name,
                                           const std::string& output_name) {
  if (tracepoints_.size()) {
    if (tracepoint_ir->language() != language_) {
      return error::InvalidArgument(
          "Cannot add '$1' tracer to '$0' tracing program. Multiple languages not supported.",
          stirling::dynamic_tracing::ir::shared::Language_Name(language_),
          stirling::dynamic_tracing::ir::shared::Language_Name(tracepoint_ir->language()));
    }
  } else {
    language_ = tracepoint_ir->language();
  }
  tracepoint_ir->SetOutputName(output_name);

  stirling::dynamic_tracing::ir::logical::TracepointDeployment::Tracepoint tracepoint_pb;
  PL_CHECK_OK(tracepoint_ir->ToProto(tracepoint_pb.mutable_program(), probe_name));
  tracepoint_pb.set_output_name(output_name);
  tracepoints_.push_back(tracepoint_pb);

  auto output = tracepoint_ir->output();
  if (!output) {
    return Status::OK();
  }
  // Upsert the output definition.
  stirling::dynamic_tracing::ir::logical::Output output_pb;
  PL_RETURN_IF_ERROR(output->ToOutputProto(&output_pb));

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
    stirling::dynamic_tracing::ir::logical::TracepointDeployment* pb) const {
  for (const auto& tracepoint : tracepoints_) {
    (*pb->add_tracepoints()) = tracepoint;
  }
  pb->set_name(name_);

  auto one_sec = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
  pb->mutable_ttl()->set_seconds(ttl_ns_ / one_sec.count());
  pb->mutable_ttl()->set_nanos(ttl_ns_ % one_sec.count());
  return Status::OK();
}

Status MutationsIR::ToProto(plannerpb::CompileMutationsResponse* pb) {
  for (const auto& [upid, program] : upid_to_program_map_) {
    auto program_pb = pb->add_mutations()->mutable_trace();
    PL_RETURN_IF_ERROR(program->ToProto(program_pb));
    auto deployment_spec = program_pb->mutable_deployment_spec();
    auto upid_pb = deployment_spec->mutable_upid();
    upid_pb->set_asid(upid.asid());
    upid_pb->set_pid(upid.pid());
    upid_pb->set_ts_ns(upid.start_ts());
  }

  // for (const auto& [binary, program] : binary_to_program_map_) {
  //   auto program_pb = pb->add_mutations()->mutable_trace();
  //   PL_RETURN_IF_ERROR(program.ToProto(program_pb));
  //   auto binary_spec = program_pb->mutable_binary_spec();
  //   binary_spec->set_path(binary);
  // }

  for (const auto& tracepoint_to_delete : TracepointsToDelete()) {
    pb->add_mutations()->mutable_delete_tracepoint()->set_name(tracepoint_to_delete);
  }

  return Status::OK();
}

void MutationsIR::EndProbe() { current_tracepoint_ = nullptr; }

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
