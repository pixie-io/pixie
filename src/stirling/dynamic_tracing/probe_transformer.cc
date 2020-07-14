#include "src/stirling/dynamic_tracing/probe_transformer.h"

#include <map>
#include <string>
#include <utility>

#include "src/stirling/dynamic_tracing/goid.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

void CreateMap(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  auto* stash_map = out->add_maps();
  stash_map->set_name(input_probe.name() + "_argstash");
}

void CreateEntryProbe(const ir::logical::Probe& input_probe, ir::logical::Program* out) {
  auto* entry_probe = out->add_probes();
  entry_probe->mutable_trace_point()->CopyFrom(input_probe.trace_point());
  entry_probe->mutable_trace_point()->set_type(ir::shared::TracePoint::ENTRY);
  entry_probe->set_name(input_probe.name() + "_entry");

  // Access arguments.
  for (const auto& in_arg : input_probe.args()) {
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
  for (const auto& in_arg : input_probe.args()) {
    stash_action->add_value_variable_name(in_arg.id());
  }
}

Status CreateReturnProbe(const ir::logical::Probe& input_probe,
                         const std::map<std::string_view, ir::shared::Output*>& outputs,
                         ir::logical::Program* out) {
  auto* return_probe = out->add_probes();
  return_probe->set_name(input_probe.name() + "_return");
  return_probe->mutable_trace_point()->CopyFrom(input_probe.trace_point());
  return_probe->mutable_trace_point()->set_type(ir::shared::TracePoint::RETURN);

  auto* map_val = return_probe->add_map_vals();
  map_val->set_map_name(input_probe.name() + "_argstash");
  // TODO(oazizi): goid is hard-coded. Fix based on language.
  map_val->set_key_expr("goid");
  for (const auto& in_arg : input_probe.args()) {
    map_val->add_value_ids(in_arg.id());
  }

  // Generate return values.
  for (const auto& in_ret_val : input_probe.ret_vals()) {
    auto* out_ret_val = return_probe->add_ret_vals();
    out_ret_val->CopyFrom(in_ret_val);
  }

  // Generate output on return probe.
  std::string output_table_name = input_probe.name() + "_table";
  auto iter = outputs.find(output_table_name);
  if (iter == outputs.end()) {
    return error::Internal("Reference to undefined table: $0", output_table_name);
  }

  int probe_output_num_fields = input_probe.args_size() + input_probe.ret_vals_size();
  if (iter->second->fields_size() != probe_output_num_fields) {
    return error::Internal("Probe output size $0 does not match Output definition size $1",
                           probe_output_num_fields, iter->second->fields_size());
  }

  auto* output_action = return_probe->add_output_actions();
  output_action->set_output_name(input_probe.name() + "_table");

  for (const auto& in_arg : input_probe.args()) {
    output_action->add_variable_name(in_arg.id());
  }

  for (const auto& in_ret_val : input_probe.ret_vals()) {
    output_action->add_variable_name(in_ret_val.id());
  }

  for (const auto& printk : input_probe.printks()) {
    return_probe->add_printks()->CopyFrom(printk);
  }

  return Status::OK();
}

Status TransformLogicalProbe(const ir::logical::Probe& input_probe,
                             const std::map<std::string_view, ir::shared::Output*>& outputs,
                             ir::logical::Program* out) {
  // A logical probe is allowed to implicitly access arguments and return values.
  // Here we expand this out to be explicit. We break the logical probe into:
  // 1) An entry probe - to grab any potential arguments.
  // 2) A return probe - to grab any potential return values.
  // 3) A map - to stash the arguments and transfer them to the return probe.
  // TODO(oazizi): An optimization could be to determine whether both entry and return probes
  //               are required. When not required, one probe and the stash map can be avoided.
  CreateMap(input_probe, out);
  CreateEntryProbe(input_probe, out);
  PL_RETURN_IF_ERROR(CreateReturnProbe(input_probe, outputs, out));

  return Status::OK();
}

StatusOr<ir::logical::Program> TransformLogicalProgram(const ir::logical::Program& input_program) {
  ir::logical::Program out;

  std::map<std::string_view, ir::shared::Output*> outputs;

  // Copy the binary path.
  out.set_binary_path(input_program.binary_path());

  // Copy all explicitly declared output buffers.
  for (const auto& o : input_program.outputs()) {
    auto* output = out.add_outputs();
    output->CopyFrom(o);
    outputs[output->name()] = output;
  }

  // Copy all explicitly declared maps.
  for (const auto& m : input_program.maps()) {
    auto* map = out.add_maps();
    map->CopyFrom(m);
  }

  if (!input_program.probes().empty()) {
    out.add_maps()->CopyFrom(GenGOIDMap());
    out.add_probes()->CopyFrom(GenGOIDProbe());
  }

  for (const auto& p : input_program.probes()) {
    if (p.trace_point().type() == ir::shared::TracePoint::LOGICAL) {
      PL_RETURN_IF_ERROR(TransformLogicalProbe(p, outputs, &out));
    } else {
      auto* probe = out.add_probes();
      probe->CopyFrom(p);
    }
  }

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
