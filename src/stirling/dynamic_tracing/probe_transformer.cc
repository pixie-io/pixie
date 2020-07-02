#include "src/stirling/dynamic_tracing/probe_transformer.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

void CreateOutput(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  auto* out_buffer = out->add_outputs();
  out_buffer->set_name(input_probe.name() + "_table");
}

void CreateMap(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  auto* stash_map = out->add_maps();
  stash_map->set_name(input_probe.name() + "_argstash");
}

void CreateEntryProbe(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  auto* entry_probe = out->add_probes();
  entry_probe->mutable_trace_point()->CopyFrom(input_probe.trace_point());
  entry_probe->mutable_trace_point()->set_type(ir::shared::TracePoint::ENTRY);
  entry_probe->set_name(input_probe.name() + "_entry");
  for (auto& in_arg : input_probe.args()) {
    auto* out_arg = entry_probe->add_args();
    out_arg->CopyFrom(in_arg);
  }

  // Generate argument stash.
  // For now, always stash all arguments.
  auto* stash_action = entry_probe->add_stash_map_actions();
  stash_action->set_map_name(input_probe.name() + "_argstash");
  // TODO(oazizi): goid is hard-coded. Fix based on language.
  stash_action->set_key_expr("goid");
  for (auto& in_arg : input_probe.args()) {
    stash_action->add_value_variable_name(in_arg.id());
  }
}

void CreateReturnProbe(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  auto* return_probe = out->add_probes();
  return_probe->set_name(input_probe.name() + "_return");
  return_probe->mutable_trace_point()->CopyFrom(input_probe.trace_point());
  return_probe->mutable_trace_point()->set_type(ir::shared::TracePoint::RETURN);
  for (auto& in_ret_val : input_probe.ret_vals()) {
    auto* out_ret_val = return_probe->add_ret_vals();
    out_ret_val->CopyFrom(in_ret_val);
  }

  // Generate output on return probe.
  auto* output_action = return_probe->add_output_actions();
  output_action->set_output_name(input_probe.name() + "_table");

  for (auto& in_arg : input_probe.args()) {
    output_action->add_variable_name(in_arg.id());
  }

  for (auto& in_ret_val : input_probe.ret_vals()) {
    output_action->add_variable_name(in_ret_val.id());
  }
}

StatusOr<ir::logical::Program> TransformLogicalProbe(const ir::logical::Probe& input_probe) {
  ir::logical::Program out;

  CreateOutput(input_probe, &out);
  CreateMap(input_probe, &out);
  CreateEntryProbe(input_probe, &out);
  CreateReturnProbe(input_probe, &out);

  // TODO(yzhao): Add GOID constructs here.
  // CreateGOIDMap(&out);
  // CreateGOIDProbe(&out);

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
