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
  auto* stash_action = entry_probe->add_map_stash_actions();
  stash_action->set_map_name(input_probe.name() + "_argstash");
  // TODO(oazizi): goid is hard-coded. Fix based on language. Non-Golang languages probably should
  // use TGID_PID.
  stash_action->set_key(ir::shared::BPFHelper::GOID);
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

void TransformLogicalProbe(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  // A logical probe is allowed to implicitly access arguments and return values.
  // Here we expand this out to be explicit. We break the logical probe into:
  // 1) An entry probe - to grab any potential arguments.
  // 2) A return probe - to grab any potential return values.
  // 3) A map - to stash the arguments and transfer them to the return probe.
  // 4) An output - to output the results from the return probe.
  // TODO(oazizi): An optimization could be to determine whether both entry and return probes
  //               are required. When not required, one probe and the stash map can be avoided.
  CreateOutput(input_probe, out);
  CreateMap(input_probe, out);
  CreateEntryProbe(input_probe, out);
  CreateReturnProbe(input_probe, out);
}

StatusOr<ir::logical::Program> TransformLogicalProgram(const ir::logical::Program& input_program) {
  ir::logical::Program out;

  // Copy all explicitly declared output buffers.
  for (const auto& o : input_program.outputs()) {
    auto* output = out.add_outputs();
    output->CopyFrom(o);
  }

  // Copy all explicitly declared maps.
  for (const auto& m : input_program.maps()) {
    auto* map = out.add_maps();
    map->CopyFrom(m);
  }

  for (const auto& p : input_program.probes()) {
    if (p.trace_point().type() == ir::shared::TracePoint::LOGICAL) {
      TransformLogicalProbe(p, &out);
    } else {
      auto* probe = out.add_probes();
      probe->CopyFrom(p);
    }
  }

  // TODO(yzhao): Add GOID constructs here.
  // CreateGOIDMap(&out);
  // CreateGOIDProbe(&out);

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
