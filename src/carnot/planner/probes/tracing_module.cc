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

#include "src/carnot/planner/probes/tracing_module.h"
#include <sole.hpp>

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/probes/kprobe_target.h"
#include "src/carnot/planner/probes/label_selector_target.h"
#include "src/carnot/planner/probes/process_target.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<ProbeObject>> ProbeObject::Create(
    const pypa::AstPtr& ast, ASTVisitor* visitor, const std::shared_ptr<TracepointIR>& probe) {
  return std::shared_ptr<ProbeObject>(new ProbeObject(ast, visitor, probe));
}

class LatencyHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

class UpsertHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

class TraceProgramHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
};

class SharedObjectHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
};

class KProbeTargetHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
};

class DeleteTracepointHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

class ReturnHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};
StatusOr<QLObjectPtr> ProcessTargetHandler(const pypa::AstPtr& ast, const ParsedArgs& args,
                                           ASTVisitor* visitor);

StatusOr<QLObjectPtr> LabelSelectorTargetHandler(const pypa::AstPtr& ast, const ParsedArgs& args,
                                                 ASTVisitor* visitor);

StatusOr<QLObjectPtr> LatencyHandler::Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                           const ParsedArgs&, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto current_probe, mutations_ir->GetCurrentProbeOrError(ast));
  std::string id = current_probe->NextLatencyName();
  current_probe->SetFunctionLatencyID(id);
  return std::static_pointer_cast<QLObject>(
      std::make_shared<TracingVariableObject>(ast, visitor, id));
}

StatusOr<std::shared_ptr<TraceModule>> TraceModule::Create(MutationsIR* mutations_ir,
                                                           ASTVisitor* ast_visitor) {
  auto tracing_module = std::shared_ptr<TraceModule>(new TraceModule(mutations_ir, ast_visitor));
  PX_RETURN_IF_ERROR(tracing_module->Init());
  return tracing_module;
}

Status TraceModule::Init() {
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> probe_fn,
      FuncObject::Create(kProbeTraceDefinition, {"fn_name"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(ProbeHandler::Probe, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(probe_fn->SetDocString(kProbeDocstring));
  AddMethod(kProbeTraceDefinition, probe_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> arg_expr_fn,
      FuncObject::Create(kArgExprID, {"expr"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(ArgumentHandler::Eval, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(arg_expr_fn->SetDocString(kArgExprDocstring));
  AddMethod(kArgExprID, arg_expr_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> ret_expr_fn,
      FuncObject::Create(kRetExprID, {"expr"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(ReturnHandler::Eval, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(ret_expr_fn->SetDocString(kRetExprDocstring));
  AddMethod(kRetExprID, ret_expr_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> latency_fn,
      FuncObject::Create(kFunctionLatencyID, {}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(LatencyHandler::Eval, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(latency_fn->SetDocString(kFunctionLatencyDocstring));
  AddMethod(kFunctionLatencyID, latency_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> upsert_fn,
      FuncObject::Create(kUpsertTraceID, {"name", "table_name", "probe_fn", "target", "ttl"}, {},
                         // TODO(philkuz/zasgar) uncomment definition when pod based upsert works.
                         // FuncObject::Create(kUpsertTracingVariable, {"name", "probe_fn",
                         // "pod_name", "binary", "ttl"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(UpsertHandler::Eval, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(upsert_fn->SetDocString(kUpsertTracepointDocstring));
  AddMethod(kUpsertTraceID, upsert_fn);

  // Add pxtrace.TraceProgram object (FuncObject)
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> program_fn,
                      FuncObject::Create(kTraceProgramID, {"program"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ true,
                                         std::bind(TraceProgramHandler::Eval, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));
  // add method to the pxtrace module, pxtrace.TraceProgram
  PX_RETURN_IF_ERROR(program_fn->SetDocString(kTraceProgramDocstring));
  AddMethod(kTraceProgramID, program_fn);

  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> shared_object_fn,
                      FuncObject::Create(kSharedObjectID, {"name", "upid"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(SharedObjectHandler::Eval, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(shared_object_fn->SetDocString(kSharedObjectDocstring));
  AddMethod(kSharedObjectID, shared_object_fn);

  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> kprobe_target_fn,
                      FuncObject::Create(kKProbeTargetID, {}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(KProbeTargetHandler::Eval, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(kprobe_target_fn->SetDocString(kKProbeTargetDocstring));
  AddMethod(kKProbeTargetID, kprobe_target_fn);

  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> delete_fn,
                      FuncObject::Create(kDeleteTracepointID, {"name"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(DeleteTracepointHandler::Eval, mutations_ir_,
                                                   std::placeholders::_1, std::placeholders::_2,
                                                   std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(delete_fn->SetDocString(kDeleteTracepointDocstring));
  AddMethod(kDeleteTracepointID, delete_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> process_target_constructor,
      FuncObject::Create(kProcessTargetID, {"pod_name", "container_name", "process_name"},
                         {{"process_name", "''"}, {"container_name", "''"}},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(ProcessTargetHandler, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(process_target_constructor->SetDocString(kProcessTargetDocstring));
  AddMethod(kProcessTargetID, process_target_constructor);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> label_selector_target_constructor,
      FuncObject::Create(kLabelSelectorTargetID,
                         {"labels", "namespace", "container_name", "process_name"},
                         {{"process_name", "''"}, {"container_name", "''"}},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(LabelSelectorTargetHandler, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(
      label_selector_target_constructor->SetDocString(kLabelSelectorTargetDocString));
  AddMethod(kLabelSelectorTargetID, label_selector_target_constructor);

  return Status::OK();
}

StatusOr<QLObjectPtr> ProbeHandler::Probe(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                          const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(mutations_ir);
  PX_ASSIGN_OR_RETURN(StringIR * function_name_ir, GetArgAs<StringIR>(ast, args, "fn_name"));

  return FuncObject::Create(
      TraceModule::kProbeTraceDefinition, {"fn"}, {},
      /* has_variable_len_args */ false,
      /* has_variable_len_kwargs */ false,
      std::bind(&ProbeHandler::Decorator, mutations_ir, function_name_ir->str(),
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      visitor);
}

StatusOr<QLObjectPtr> ProbeHandler::Decorator(MutationsIR* mutations_ir,
                                              const std::string& function_name,
                                              const pypa::AstPtr& ast, const ParsedArgs& args,
                                              ASTVisitor* visitor) {
  auto fn = args.GetArg("fn");
  PX_ASSIGN_OR_RETURN(auto func, GetCallMethod(ast, fn));
  // mutations_ir->AddFunc(func);
  // Need to wrap the call of the method to "start" the probe
  return FuncObject::Create(
      "wrapper", {}, {},
      /* has_variable_len_args */ false,
      /* has_variable_len_kwargs */ false,
      std::bind(&ProbeHandler::Wrapper, mutations_ir, function_name, func, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
      visitor);
}

Status ParseColumns(TracepointIR* probe, CollectionObject* column_object) {
  std::vector<std::string> col_names;
  std::vector<std::string> var_names;
  for (const auto& item : column_object->items()) {
    if (!DictObject::IsDict(item)) {
      return item->CreateError("Expected Dict, got $0", item->name());
    }
    auto dict = static_cast<DictObject*>(item.get());
    auto values = dict->values();
    auto keys = dict->keys();
    DCHECK_EQ(values.size(), keys.size());

    for (const auto& [idx, key] : Enumerate(keys)) {
      PX_ASSIGN_OR_RETURN(auto key_str_ir, GetArgAs<StringIR>(key, "key"));

      auto value = values[idx];
      if (!TracingVariableObject::IsTracingVariable(value)) {
        return value->CreateError("Expected TracingVariable, got $0", value->name());
      }
      auto probe = static_cast<TracingVariableObject*>(value.get());
      col_names.push_back(key_str_ir->str());
      var_names.push_back(probe->id());
    }
  }
  probe->CreateNewOutput(col_names, var_names);
  return Status::OK();
}

Status ParseOutput(TracepointIR* probe, const QLObjectPtr& probe_output) {
  DCHECK(probe);
  if (probe_output->type() == QLObjectType::kNone) {
    // Do nothing.
    return Status::OK();
  }
  if (!CollectionObject::IsCollection(probe_output)) {
    return probe_output->CreateError(
        "Unable to parse probe output definition. Expected Collection, received $0",
        probe_output->name());
  }
  auto columns = static_cast<CollectionObject*>(probe_output.get());

  return ParseColumns(probe, columns);
}

StatusOr<QLObjectPtr> ProbeHandler::Wrapper(MutationsIR* mutations_ir,
                                            const std::string& function_name,
                                            const std::shared_ptr<FuncObject> wrapped_func,
                                            const pypa::AstPtr& ast, const ParsedArgs&,
                                            ASTVisitor* visitor) {
  if (mutations_ir->current_probe() != nullptr) {
    return CreateAstError(ast,
                          "Already have a current probe. Are you calling this in a another trace "
                          "definition.");
  }
  auto probe = mutations_ir->StartProbe(function_name);
  // Note that even though we call the wrapped func here, Handler::Wrapper only gets called
  // whenever the resulting funcobject is called. Ie in pxtrace.Upsert.
  PX_ASSIGN_OR_RETURN(auto wrapped_result, wrapped_func->Call({}, ast));
  PX_RETURN_IF_ERROR(ParseOutput(probe.get(), wrapped_result));
  // If this doesn't have any output, we error out. In the future, we'll allow mutations_ir
  // that don't produce an output iff they interact with BPF maps.
  if (probe->output() == nullptr) {
    return wrapped_result->CreateError(
        "Improper probe definition: missing output spec of probe, add a return statement");
  }

  mutations_ir->EndProbe();
  PX_ASSIGN_OR_RETURN(auto probe_obj, ProbeObject::Create(ast, visitor, probe));
  return std::static_pointer_cast<QLObject>(probe_obj);
}

StatusOr<QLObjectPtr> ArgumentHandler::Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                            const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(mutations_ir);
  PX_ASSIGN_OR_RETURN(auto current_probe, mutations_ir->GetCurrentProbeOrError(ast));

  PX_ASSIGN_OR_RETURN(auto expr_ir, GetArgAs<StringIR>(ast, args, "expr"));
  std::string id = current_probe->NextArgName();
  current_probe->AddArgument(id, expr_ir->str());

  return std::static_pointer_cast<QLObject>(
      std::make_shared<TracingVariableObject>(ast, visitor, id));
}

StatusOr<QLObjectPtr> ReturnHandler::Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                          const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(mutations_ir);
  PX_ASSIGN_OR_RETURN(auto current_probe, mutations_ir->GetCurrentProbeOrError(ast));

  PX_ASSIGN_OR_RETURN(auto expr_ir, GetArgAs<StringIR>(ast, args, "expr"));
  std::string id = current_probe->NextReturnName();
  current_probe->AddReturnValue(id, expr_ir->str());

  return std::static_pointer_cast<QLObject>(
      std::make_shared<TracingVariableObject>(ast, visitor, id));
}

// Construct a TraceProgram object
StatusOr<QLObjectPtr> TraceProgramHandler::Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                                ASTVisitor* visitor) {
  std::vector<TracepointSelector> selectors;
  // Check if supported selectors are passed in kwargs
  const std::vector<NameToNode>& kwargs = args.kwargs();
  const google::protobuf::EnumDescriptor* selector_type_descriptor =
      carnot::planner::dynamic_tracing::ir::logical::SelectorType_descriptor();
  for (const auto& [name, node] : kwargs) {
    if (name == "program") {
      continue;
    }
    // Enums are stored in uppercase, so we convert the argument key
    const google::protobuf::EnumValueDescriptor* selector_value =
        selector_type_descriptor->FindValueByName(absl::AsciiStrToUpper(name));
    if (selector_value) {
      // Selector type found
      carnot::planner::dynamic_tracing::ir::logical::TracepointSelector tracepoint_selector;
      tracepoint_selector.set_selector_type(
          static_cast<carnot::planner::dynamic_tracing::ir::logical::SelectorType>(
              selector_value->number()));
      // Set user provided restriction, taken from the argument value
      PX_ASSIGN_OR_RETURN(auto selector_value_ir, GetArgAs<StringIR>(node, name));
      // Selector value is empty
      if (selector_value_ir->str().empty()) {
        return CreateAstError(ast, "Empty selector value provided for '$0'", name);
      }
      tracepoint_selector.set_value(selector_value_ir->str());
      selectors.push_back(tracepoint_selector);
    } else {
      return CreateAstError(ast, "Unsupported selector argument provided '$0'", name);
    }
  }
  // extract BPFTrace program string
  PX_ASSIGN_OR_RETURN(auto program_ir, GetArgAs<StringIR>(ast, args, "program"));
  return std::static_pointer_cast<QLObject>(
      std::make_shared<TraceProgramObject>(ast, visitor, program_ir->str(), selectors));
}

StatusOr<QLObjectPtr> UpsertHandler::Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                          const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(mutations_ir);

  PX_ASSIGN_OR_RETURN(auto tp_deployment_name_ir, GetArgAs<StringIR>(ast, args, "name"));
  PX_ASSIGN_OR_RETURN(auto output_name_ir, GetArgAs<StringIR>(ast, args, "table_name"));
  // TODO(philkuz) support pod_name
  // PX_ASSIGN_OR_RETURN(auto pod_name_ir, GetArgAs<StringIR>(args, "pod_name"));
  // PX_ASSIGN_OR_RETURN(auto binary_name_ir, GetArgAs<StringIR>(args, "binary"));
  PX_ASSIGN_OR_RETURN(auto ttl_ir, GetArgAs<StringIR>(ast, args, "ttl"));

  const std::string& tp_deployment_name = tp_deployment_name_ir->str();
  const std::string& output_name = output_name_ir->str();
  PX_ASSIGN_OR_RETURN(int64_t ttl_ns, StringToTimeInt(ttl_ir->str()));

  // TODO(philkuz/oazizi/zasgar) when we support pods and so on, add this back in.
  // const auto& pod_name = pod_name_ir->str();
  // const auto& container_name = pod_name_ir->str();
  // const auto& binary_name = binary_name_ir->str();
  TracepointDeployment* trace_deployment;
  auto target = args.GetArg("target");
  if (SharedObjectTarget::IsSharedObject(target)) {
    auto shared_object = std::static_pointer_cast<SharedObjectTarget>(target);
    auto trace_deployment_or_s = mutations_ir->CreateTracepointDeployment(
        tp_deployment_name, shared_object->shared_object(), ttl_ns);
    PX_RETURN_IF_ERROR(WrapAstError(ast, trace_deployment_or_s.status()));
    trace_deployment = trace_deployment_or_s.ConsumeValueOrDie();
  } else if (KProbeTarget::IsKProbeTarget(target)) {
    auto trace_deployment_or_s =
        mutations_ir->CreateKProbeTracepointDeployment(tp_deployment_name, ttl_ns);
    PX_RETURN_IF_ERROR(WrapAstError(ast, trace_deployment_or_s.status()));
    trace_deployment = trace_deployment_or_s.ConsumeValueOrDie();
  } else if (ProcessTarget::IsProcessTarget(target)) {
    auto process_target = std::static_pointer_cast<ProcessTarget>(target);
    auto trace_deployment_or_s = mutations_ir->CreateTracepointDeploymentOnProcessSpec(
        tp_deployment_name, process_target->target(), ttl_ns);
    PX_RETURN_IF_ERROR(WrapAstError(ast, trace_deployment_or_s.status()));
    trace_deployment = trace_deployment_or_s.ConsumeValueOrDie();
  } else if (LabelSelectorTarget::IsLabelSelectorTarget(target)) {
    auto label_selector_target = std::static_pointer_cast<LabelSelectorTarget>(target);
    auto trace_deployment_or_s = mutations_ir->CreateTracepointDeploymentOnLabelSelectorSpec(
        tp_deployment_name, label_selector_target->target(), ttl_ns);
    PX_RETURN_IF_ERROR(WrapAstError(ast, trace_deployment_or_s.status()));
    trace_deployment = trace_deployment_or_s.ConsumeValueOrDie();
  } else if (ExprObject::IsExprObject(target)) {
    auto expr_object = std::static_pointer_cast<ExprObject>(target);
    if (Match(expr_object->expr(), UInt128Value())) {
      PX_ASSIGN_OR_RETURN(UInt128IR * upid_ir, GetArgAs<UInt128IR>(ast, args, "target"));
      md::UPID upid(upid_ir->val());

      auto trace_deployment_or_s =
          mutations_ir->CreateTracepointDeployment(tp_deployment_name, upid, ttl_ns);
      PX_RETURN_IF_ERROR(WrapAstError(ast, trace_deployment_or_s.status()));
      trace_deployment = trace_deployment_or_s.ConsumeValueOrDie();
    } else {
      return CreateAstError(ast, "Unexpected type '$0' for arg '$1'",
                            expr_object->expr()->type_string(), "target");
    }
  } else {
    return CreateAstError(ast, "Unexpected type '$0' for arg '$1'",
                          QLObjectTypeString(target->type()), "target");
  }

  // looking at probe_fn arg of the UpsertTracepoint function and check what kind of object it is
  // (Checking for FuncObject is a legacy thing, usually it's a bpftrace program as a string)
  if (FuncObject::IsFuncObject(args.GetArg("probe_fn"))) {
    PX_ASSIGN_OR_RETURN(auto probe_fn, GetCallMethod(ast, args.GetArg("probe_fn")));
    PX_ASSIGN_OR_RETURN(auto probe, probe_fn->Call({}, ast));
    CHECK(ProbeObject::IsProbe(probe));
    auto probe_ir = std::static_pointer_cast<ProbeObject>(probe)->probe();
    PX_RETURN_IF_ERROR(WrapAstError(
        ast, trace_deployment->AddTracepoint(probe_ir.get(), tp_deployment_name, output_name)));
    // If passing UpsertTracepoint a TraceProgram object or a list of TraceProgram objects,
    // then we add the bpftrace script string and we create selectors from
    // the arguments on the TraceProgram object, populating TracepointDeployment.selectors
  } else if (CollectionObject::IsCollection(args.GetArg("probe_fn"))) {
    // The probe_fn (QL object) is a list of TraceProgram objects.
    // for each of the TraceProgram objects in the list, add the bpftrace script and selectors
    for (const auto& item :
         static_cast<CollectionObject*>(args.GetArg("probe_fn").get())->items()) {
      if (!TraceProgramObject::IsTraceProgram(item)) {
        return item->CreateError("Expected TraceProgram, got $0", item->name());
      }
      auto trace_program = static_cast<TraceProgramObject*>(item.get());
      PX_RETURN_IF_ERROR(
          WrapAstError(ast, trace_deployment->AddBPFTrace(trace_program->program(), output_name,
                                                          trace_program->selectors())));
    }
  } else if (TraceProgramObject::IsTraceProgram(args.GetArg("probe_fn"))) {
    // The probe_fn (QL object) is a single TraceProgram object.
    auto trace_program = static_cast<TraceProgramObject*>(args.GetArg("probe_fn").get());
    PX_RETURN_IF_ERROR(
        WrapAstError(ast, trace_deployment->AddBPFTrace(trace_program->program(), output_name,
                                                        trace_program->selectors())));
  } else {
    // The probe_fn is a string.
    PX_ASSIGN_OR_RETURN(auto program_str_ir, GetArgAs<StringIR>(ast, args, "probe_fn"));
    std::vector<TracepointSelector> empty_selectors;
    PX_RETURN_IF_ERROR(WrapAstError(
        ast, trace_deployment->AddBPFTrace(program_str_ir->str(), output_name, empty_selectors)));
  }

  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ast, visitor));
}

StatusOr<QLObjectPtr> SharedObjectHandler::Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                                ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto shared_object_name_ir, GetArgAs<StringIR>(ast, args, "name"));
  PX_ASSIGN_OR_RETURN(UInt128IR * upid_ir, GetArgAs<UInt128IR>(ast, args, "upid"));
  std::string shared_object_name = shared_object_name_ir->str();
  md::UPID shared_object_upid(upid_ir->val());

  return SharedObjectTarget::Create(ast, visitor, shared_object_name, shared_object_upid);
}

StatusOr<QLObjectPtr> KProbeTargetHandler::Eval(const pypa::AstPtr& ast, const ParsedArgs&,
                                                ASTVisitor* visitor) {
  return KProbeTarget::Create(ast, visitor);
}

StatusOr<QLObjectPtr> DeleteTracepointHandler::Eval(MutationsIR* mutations_ir,
                                                    const pypa::AstPtr& ast, const ParsedArgs& args,
                                                    ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto tp_deployment_name_ir, GetArgAs<StringIR>(ast, args, "name"));
  const std::string& tp_deployment_name = tp_deployment_name_ir->str();
  mutations_ir->DeleteTracepoint(tp_deployment_name);
  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ast, visitor));
}

StatusOr<QLObjectPtr> ProcessTargetHandler(const pypa::AstPtr& ast, const ParsedArgs& args,
                                           ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto pod_name_ir, GetArgAs<StringIR>(ast, args, "pod_name"));
  PX_ASSIGN_OR_RETURN(auto container_name_ir, GetArgAs<StringIR>(ast, args, "container_name"));
  PX_ASSIGN_OR_RETURN(auto process_path_ir, GetArgAs<StringIR>(ast, args, "process_name"));
  return ProcessTarget::Create(ast, visitor, pod_name_ir->str(), container_name_ir->str(),
                               process_path_ir->str());
}

StatusOr<QLObjectPtr> LabelSelectorTargetHandler(const pypa::AstPtr& ast, const ParsedArgs& args,
                                                 ASTVisitor* visitor) {
  QLObjectPtr labels_ir = args.GetArg("labels");
  if (!DictObject::IsDict(labels_ir)) {
    return labels_ir->CreateError("Expected labels to be a dictionary, received $0",
                                  labels_ir->name());
  }
  PX_ASSIGN_OR_RETURN(auto namespace_ir, GetArgAs<StringIR>(ast, args, "namespace"));
  PX_ASSIGN_OR_RETURN(auto container_name_ir, GetArgAs<StringIR>(ast, args, "container_name"));
  PX_ASSIGN_OR_RETURN(auto process_path_ir, GetArgAs<StringIR>(ast, args, "process_name"));

  // Parse Labels into a map.
  auto labels_dict = static_cast<DictObject*>(labels_ir.get());
  auto values = labels_dict->values();
  auto keys = labels_dict->keys();
  DCHECK_EQ(values.size(), keys.size());
  absl::flat_hash_map<std::string, std::string> labels;
  for (size_t i = 0; i < keys.size(); ++i) {
    PX_ASSIGN_OR_RETURN(auto key, GetArgAs<StringIR>(keys[i], "label_key"));
    PX_ASSIGN_OR_RETURN(auto value, GetArgAs<StringIR>(values[i], "label_value"));
    labels[key->str()] = value->str();
  }

  return LabelSelectorTarget::Create(ast, visitor, labels, namespace_ir->str(),
                                     container_name_ir->str(), process_path_ir->str());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
