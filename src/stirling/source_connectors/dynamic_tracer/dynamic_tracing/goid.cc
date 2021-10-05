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

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/goid.h"

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dwarvifier.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

ir::shared::Map GenGOIDMap() {
  ir::shared::Map map;
  map.set_name("pid_goid_map");
  return map;
}

// Returns an intermediate probe that traces creation of go routine.
ir::logical::Probe GenGOIDProbe() {
  ir::logical::Probe probe;

  // probe.tracepoint.binary is not set. It's left for caller to attach to the same binary,
  // whose other probes need to access goid.

  probe.set_name("probe_entry_runtime_casgstatus");

  probe.mutable_tracepoint()->set_symbol("runtime.casgstatus");
  probe.mutable_tracepoint()->set_type(ir::shared::Tracepoint::ENTRY);

  auto* constant = probe.add_consts();
  constant->set_name("kGRunningState");
  constant->set_type(ir::shared::ScalarType::INT64);
  // 2 indicates a new goid has been created, so it should be recorded in the map.
  constant->set_constant("2");

  auto* goid_arg = probe.add_args();
  goid_arg->set_id("goid");
  goid_arg->set_expr("gp.goid");

  auto* newval_arg = probe.add_args();
  newval_arg->set_id("newval");
  newval_arg->set_expr("newval");

  auto* map_stash_action = probe.add_map_stash_actions();

  map_stash_action->set_map_name("pid_goid_map");
  map_stash_action->set_key(ir::shared::BPFHelper::TGID_PID);
  map_stash_action->add_value_variable_names("goid");
  map_stash_action->mutable_cond()->set_op(ir::shared::Condition::EQUAL);
  map_stash_action->mutable_cond()->add_vars("newval");
  map_stash_action->mutable_cond()->add_vars("kGRunningState");

  return probe;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
