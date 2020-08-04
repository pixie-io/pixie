#include "src/carnot/planner/probes/probes.h"

#include <utility>

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

Status ProbeIR::ToProto(stirling::dynamic_tracing::ir::logical::Probe* pb) {
  auto* trace_point_pb = pb->mutable_trace_point();
  trace_point_pb->set_symbol(symbol_);
  trace_point_pb->set_type(stirling::dynamic_tracing::ir::shared::TracePoint::LOGICAL);

  for (const auto& arg : args_) {
    *pb->add_args() = arg;
  }

  for (const auto& retval : ret_vals_) {
    *pb->add_ret_vals() = retval;
  }

  if (HasLatencyCol()) {
    pb->mutable_function_latency()->set_id(latency_col_id_);
  }

  // TODO(philkuz) implement map methods.
  // for (const auto& map_stash : map_stash_actions_) {
  // (*pb->add_map_stash_actions()) = map_stash;
  // }

  // Probes don't necessarily have an output. IE if our probe just writes to a map.
  if (output_) {
    PL_RETURN_IF_ERROR(output_->ToActionProto(pb->add_output_actions()));
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

void ProbeIR::SetOutputName(const std::string& output_name) {
  if (!output_) {
    return;
  }
  output_->set_name(output_name);
}

void ProbeIR::CreateNewOutput(const std::vector<std::string>& col_names,
                              const std::vector<std::string>& var_names) {
  output_ = std::make_shared<ProbeOutput>(col_names, var_names);
}

void ProbeIR::AddArgument(const std::string& id, const std::string& expr) {
  stirling::dynamic_tracing::ir::logical::Argument arg;
  arg.set_id(id);
  arg.set_expr(expr);
  args_.push_back(arg);
}

void ProbeIR::AddReturnValue(const std::string& id, const std::string& expr) {
  stirling::dynamic_tracing::ir::logical::ReturnValue ret;
  ret.set_id(id);
  // TODO(philkuz/oazizi) The expression needs to be in the form "$<index>.<field>.<...>".
  ret.set_expr(expr);
  ret_vals_.push_back(ret);
}

std::shared_ptr<ProbeIR> DynamicTraceIR::StartProbe(
    stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
    const std::string& function_name) {
  auto probe_ir = std::make_shared<ProbeIR>(language, function_name);
  probes_pool_.push_back(probe_ir);
  current_probe_ = probe_ir;
  return probe_ir;
}

StatusOr<TracingProgram*> DynamicTraceIR::CreateTraceProgram(const std::string& trace_point_name,
                                                             const md::UPID& upid, int64_t ttl_ns) {
  if (!upid_to_program_map_.empty() && upid_to_program_map_.contains(upid)) {
    return error::InvalidArgument(
        "Cannot UpsertTracepoint on the same binary. Use UpsertTracepoints instead.");
  }
  std::unique_ptr<TracingProgram> program =
      std::make_unique<TracingProgram>(trace_point_name, ttl_ns);
  TracingProgram* raw = program.get();
  upid_to_program_map_[upid] = std::move(program);
  return raw;
}

Status TracingProgram::AddProbe(ProbeIR* probe_ir, const std::string& probe_name,
                                const std::string& output_name) {
  if (probes_.size()) {
    if (probe_ir->language() != language_) {
      return error::InvalidArgument(
          "Cannot add '$1' tracer to '$0' tracing program. Multiple languages not supported.",
          stirling::dynamic_tracing::ir::shared::BinarySpec_Language_Name(language_),
          stirling::dynamic_tracing::ir::shared::BinarySpec_Language_Name(probe_ir->language()));
    }
  } else {
    language_ = probe_ir->language();
  }
  probe_ir->SetOutputName(output_name);

  stirling::dynamic_tracing::ir::logical::Probe probe_pb;
  PL_CHECK_OK(probe_ir->ToProto(&probe_pb));
  probe_pb.set_name(probe_name);
  probes_.push_back(probe_pb);

  auto output = probe_ir->output();
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

StatusOr<ProbeIR*> DynamicTraceIR::GetCurrentProbeOrError(const pypa::AstPtr& ast) {
  if (current_probe_.get() == nullptr) {
    return CreateAstError(ast, "Missing current probe");
  }
  return current_probe_.get();
}

Status TracingProgram::ToProto(stirling::dynamic_tracing::ir::logical::Program* pb) const {
  auto binary_spec = pb->mutable_binary_spec();
  // TODO(philkuz/oazizi) need to pass in from query.
  binary_spec->set_language(language_);
  for (const auto& probe : probes_) {
    (*pb->add_probes()) = probe;
  }
  for (const auto& output : outputs_) {
    (*pb->add_outputs()) = output;
  }
  pb->set_name(name_);

  auto one_sec = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1));
  pb->mutable_ttl()->set_seconds(ttl_ns_ / one_sec.count());
  pb->mutable_ttl()->set_nanos(ttl_ns_ % one_sec.count());
  return Status::OK();
}

Status DynamicTraceIR::ToProto(plannerpb::CompileMutationsResponse* pb) {
  for (const auto& [upid, program] : upid_to_program_map_) {
    auto program_pb = pb->add_mutations()->mutable_trace();
    PL_RETURN_IF_ERROR(program->ToProto(program_pb));
    auto binary_spec = program_pb->mutable_binary_spec();
    auto upid_pb = binary_spec->mutable_upid();
    upid_pb->set_asid(upid.asid());
    upid_pb->set_pid(upid.pid());
    upid_pb->set_ts_ns(upid.start_ts());
  }

  for (const auto& [binary, program] : binary_to_program_map_) {
    auto program_pb = pb->add_mutations()->mutable_trace();
    PL_RETURN_IF_ERROR(program.ToProto(program_pb));
    auto binary_spec = program_pb->mutable_binary_spec();
    binary_spec->set_path(binary);
  }
  return Status::OK();
}

void DynamicTraceIR::EndProbe() { current_probe_ = nullptr; }

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
