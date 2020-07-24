#include "src/carnot/planner/probes/tracing_module.h"

#include <sole.hpp>

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/none_object.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<ProbeObject>> ProbeObject::Create(ASTVisitor* visitor,
                                                           const std::shared_ptr<ProbeIR>& probe) {
  return std::shared_ptr<ProbeObject>(new ProbeObject(visitor, probe));
}

class LatencyHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

class UpsertHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

class DeleteTracepointHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

class ReturnHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

StatusOr<QLObjectPtr> LatencyHandler::Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                           const ParsedArgs&, ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(auto current_probe, probes->GetCurrentProbeOrError(ast));
  std::string id = current_probe->NextLatencyName();
  current_probe->SetFunctionLatencyID(id);
  return std::static_pointer_cast<QLObject>(std::make_shared<TracingVariableObject>(visitor, id));
}

StatusOr<std::shared_ptr<TraceModule>> TraceModule::Create(DynamicTraceIR* probes,
                                                           ASTVisitor* ast_visitor) {
  auto tracing_module = std::shared_ptr<TraceModule>(new TraceModule(probes, ast_visitor));
  PL_RETURN_IF_ERROR(tracing_module->Init());
  return tracing_module;
}

Status TraceModule::Init() {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> probe_fn,
      FuncObject::Create(
          kGoProbeTraceDefinition, {"fn_name"}, {},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(ProbeHandler::Probe, probes_,
                    stirling::dynamic_tracing::ir::shared::BinarySpec::GOLANG,
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));

  AddMethod(kGoProbeTraceDefinition, probe_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> arg_expr_fn,
      FuncObject::Create(kArgumentId, {"expr"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(ArgumentHandler::Eval, probes_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  AddMethod(kArgumentId, arg_expr_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> ret_expr_fn,
      FuncObject::Create(kRetExprId, {"expr"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(ReturnHandler::Eval, probes_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  AddMethod(kRetExprId, ret_expr_fn);
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> latency_fn,
      FuncObject::Create(kFunctionLatencyId, {}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(LatencyHandler::Eval, probes_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  AddMethod(kFunctionLatencyId, latency_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> upsert_fn,
      FuncObject::Create(kUpsertTraceID, {"name", "table_name", "probe_fn", "upid", "ttl"}, {},
                         // TODO(philkuz/zasgar) uncomment definition when pod based upsert works.
                         // FuncObject::Create(kUpsertTracingVariable, {"name", "probe_fn",
                         // "pod_name", "binary", "ttl"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(UpsertHandler::Eval, probes_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  AddMethod(kUpsertTraceID, upsert_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> delete_fn,
      FuncObject::Create(kDeleteTracepointID, {"name"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(DeleteTracepointHandler::Eval, probes_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  AddMethod(kDeleteTracepointID, delete_fn);

  return Status::OK();
}

StatusOr<QLObjectPtr> ProbeHandler::Probe(
    DynamicTraceIR* probes, stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
    const pypa::AstPtr&, const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(probes);
  PL_ASSIGN_OR_RETURN(StringIR * function_name_ir, GetArgAs<StringIR>(args, "fn_name"));

  return FuncObject::Create(
      TraceModule::kGoProbeTraceDefinition, {"fn"}, {},
      /* has_variable_len_args */ false,
      /* has_variable_len_kwargs */ false,
      std::bind(&ProbeHandler::Decorator, probes, language, function_name_ir->str(),
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      visitor);
}

StatusOr<QLObjectPtr> ProbeHandler::Decorator(
    DynamicTraceIR* probes, stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
    const std::string& function_name, const pypa::AstPtr& ast, const ParsedArgs& args,
    ASTVisitor* visitor) {
  auto fn = args.GetArg("fn");
  PL_ASSIGN_OR_RETURN(auto func, GetCallMethod(ast, fn));
  // probes->AddFunc(func);
  // Need to wrap the call of the method to "start" the probe
  return FuncObject::Create(
      "wrapper", {}, {},
      /* has_variable_len_args */ false,
      /* has_variable_len_kwargs */ false,
      std::bind(&ProbeHandler::Wrapper, probes, language, function_name, func,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      visitor);
}

std::string ObjectName(const QLObjectPtr& ptr) {
  if (ptr->HasNode()) {
    return ptr->node()->type_string();
  }
  return std::string(absl::StripPrefix(magic_enum::enum_name(ptr->type()), "k"));
}

Status ParseColumns(ProbeIR* probe, CollectionObject* column_object) {
  std::vector<std::string> col_names;
  std::vector<std::string> var_names;
  for (const auto& item : column_object->items()) {
    if (!DictObject::IsDict(item)) {
      return item->CreateError("Expected Dict, got $0", ObjectName(item));
    }
    auto dict = static_cast<DictObject*>(item.get());
    auto values = dict->values();
    auto keys = dict->keys();
    DCHECK_EQ(values.size(), keys.size());

    for (const auto& [idx, key] : Enumerate(keys)) {
      PL_ASSIGN_OR_RETURN(auto key_str_ir, GetArgAs<StringIR>(key, "key"));

      auto value = values[idx];
      if (!TracingVariableObject::IsTracingVariable(value)) {
        return value->CreateError("Expected tracing variable, got $0", ObjectName(value));
      }
      auto probe = static_cast<TracingVariableObject*>(value.get());
      col_names.push_back(key_str_ir->str());
      var_names.push_back(probe->id());
    }
  }
  probe->CreateNewOutput(col_names, var_names);
  return Status::OK();
}

Status ParseOutput(ProbeIR* probe, const QLObjectPtr& probe_output) {
  DCHECK(probe);
  if (probe_output->type() == QLObjectType::kNone) {
    // Do nothing.
    return Status::OK();
  }
  if (!CollectionObject::IsCollection(probe_output)) {
    return probe_output->CreateError(
        "Unable to parse probe output definition. Expected Collection, received $0",
        ObjectName(probe_output));
  }
  auto columns = static_cast<CollectionObject*>(probe_output.get());

  return ParseColumns(probe, columns);
}

StatusOr<QLObjectPtr> ProbeHandler::Wrapper(
    DynamicTraceIR* probes, stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
    const std::string& function_name, const std::shared_ptr<FuncObject> wrapped_func,
    const pypa::AstPtr& ast, const ParsedArgs&, ASTVisitor* visitor) {
  if (probes->current_probe() != nullptr) {
    return CreateAstError(
        ast, "Already have a current probe. Are you calling this in a another trace definition.");
  }
  auto probe = probes->StartProbe(language, function_name);
  // Note that even though we call the wrapped func here, Handler::Wrapper only gets called
  // whenever the resulting funcobject is called. Ie in pxtrace.Upsert.
  PL_ASSIGN_OR_RETURN(auto wrapped_result, wrapped_func->Call({}, ast));
  PL_RETURN_IF_ERROR(ParseOutput(probe.get(), wrapped_result));
  // If this doesn't have any output, we error out. In the future, we'll allow probes
  // that don't produce an output iff they interact with BPF maps.
  if (probe->output() == nullptr) {
    return wrapped_result->CreateError(
        "Improper probe definition: missing output spec of probe, add a return statement");
  }

  probes->EndProbe();
  PL_ASSIGN_OR_RETURN(auto probe_obj, ProbeObject::Create(visitor, probe));
  return std::static_pointer_cast<QLObject>(probe_obj);
}

StatusOr<QLObjectPtr> ArgumentHandler::Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                            const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(probes);
  PL_ASSIGN_OR_RETURN(auto current_probe, probes->GetCurrentProbeOrError(ast));

  PL_ASSIGN_OR_RETURN(auto expr_ir, GetArgAs<StringIR>(args, "expr"));
  std::string id = current_probe->NextArgName();
  current_probe->AddArgument(id, expr_ir->str());

  return std::static_pointer_cast<QLObject>(std::make_shared<TracingVariableObject>(visitor, id));
}

StatusOr<QLObjectPtr> ReturnHandler::Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                          const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(probes);
  PL_ASSIGN_OR_RETURN(auto current_probe, probes->GetCurrentProbeOrError(ast));

  PL_ASSIGN_OR_RETURN(auto expr_ir, GetArgAs<StringIR>(args, "expr"));
  std::string id = current_probe->NextReturnName();
  current_probe->AddReturnValue(id, expr_ir->str());

  return std::static_pointer_cast<QLObject>(std::make_shared<TracingVariableObject>(visitor, id));
}

StatusOr<QLObjectPtr> UpsertHandler::Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                          const ParsedArgs& args, ASTVisitor* visitor) {
  DCHECK(probes);

  PL_ASSIGN_OR_RETURN(auto probe_name_ir, GetArgAs<StringIR>(args, "name"));
  PL_ASSIGN_OR_RETURN(auto output_name_ir, GetArgAs<StringIR>(args, "table_name"));
  PL_ASSIGN_OR_RETURN(UInt128IR * upid_ir, GetArgAs<UInt128IR>(args, "upid"));
  // TODO(philkuz) support pod_name
  // PL_ASSIGN_OR_RETURN(auto pod_name_ir, GetArgAs<StringIR>(args, "pod_name"));
  // PL_ASSIGN_OR_RETURN(auto binary_name_ir, GetArgAs<StringIR>(args, "binary"));
  PL_ASSIGN_OR_RETURN(auto ttl_ir, GetArgAs<StringIR>(args, "ttl"));

  const std::string& probe_name = probe_name_ir->str();
  const std::string& output_name = output_name_ir->str();
  md::UPID upid(upid_ir->val());
  PL_ASSIGN_OR_RETURN(int64_t ttl_ns, StringToTimeInt(ttl_ir->str()));

  // TODO(philkuz/oazizi/zasgar) when we support pods and so on, add this back in.
  // const auto& pod_name = pod_name_ir->str();
  // const auto& container_name = pod_name_ir->str();
  // const auto& binary_name = binary_name_ir->str();

  auto trace_program_or_s = probes->CreateTraceProgram(probe_name, upid, ttl_ns);
  PL_RETURN_IF_ERROR(WrapAstError(ast, trace_program_or_s.status()));
  auto trace_program = trace_program_or_s.ConsumeValueOrDie();

  PL_ASSIGN_OR_RETURN(auto probe_fn, GetCallMethod(ast, args.GetArg("probe_fn")));
  PL_ASSIGN_OR_RETURN(auto probe, probe_fn->Call({}, ast));
  CHECK(ProbeObject::IsProbe(probe));
  auto probe_ir = std::static_pointer_cast<ProbeObject>(probe)->probe();
  PL_RETURN_IF_ERROR(
      WrapAstError(ast, trace_program->AddProbe(probe_ir.get(), probe_name, output_name)));

  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ast, visitor));
}

StatusOr<QLObjectPtr> DeleteTracepointHandler::Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                                    const ParsedArgs& args, ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(auto probe_name_ir, GetArgAs<StringIR>(args, "name"));
  const std::string& probe_name = probe_name_ir->str();
  probes->DeleteTracepoint(probe_name);
  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ast, visitor));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
