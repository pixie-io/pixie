#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/probes/probes.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief TracingVariable holds the reference for a variable used in tracing (ie argument, return
 * value or latency).
 *
 */
class TracingVariableObject : public QLObject {
 public:
  static constexpr TypeDescriptor TracingVariableObjectType = {
      /* name */ "Tracing Variable",
      /* type */ QLObjectType::kTracingVariable,
  };

  static bool IsTracingVariable(const QLObjectPtr& ptr) {
    return ptr->type() == TracingVariableObjectType.type();
  }

  // The reference for this tracing variable.
  const std::string& id() const { return id_; }

  explicit TracingVariableObject(ASTVisitor* visitor, const std::string& id)
      : QLObject(TracingVariableObjectType, visitor), id_(id) {}

 private:
  std::string id_;
};

/**
 * @brief ProbeObject is the QLObject that wraps a probe.
 *
 */
class ProbeObject : public QLObject {
 public:
  static constexpr TypeDescriptor ProbeObjectType = {
      /* name */ "probe",
      /* type */ QLObjectType::kProbe,
  };

  static StatusOr<std::shared_ptr<ProbeObject>> Create(ASTVisitor* visitor,
                                                       const std::shared_ptr<ProbeIR>& probe);

  static bool IsProbe(const QLObjectPtr& ptr) { return ptr->type() == ProbeObjectType.type(); }
  std::shared_ptr<ProbeIR> probe() const { return probe_; }

 private:
  ProbeObject(ASTVisitor* visitor, const std::shared_ptr<ProbeIR>& probe)
      : QLObject(ProbeObjectType, visitor), probe_(probe) {}

  std::shared_ptr<ProbeIR> probe_;
};

class TraceModule : public QLObject {
 public:
  static constexpr TypeDescriptor TraceModuleType = {
      /* name */ "pxtrace",
      /* type */ QLObjectType::kTraceModule,
  };
  static StatusOr<std::shared_ptr<TraceModule>> Create(DynamicTraceIR* probes,
                                                       ASTVisitor* ast_visitor);

  // Constant for the modules.
  inline static constexpr char kTraceModuleObjName[] = "pxtrace";

  // Constants for functions of pxtrace.
  inline static constexpr char kArgumentId[] = "ArgExpr";
  inline static constexpr char kRetExprId[] = "RetExpr";
  inline static constexpr char kFunctionLatencyId[] = "FunctionLatency";
  inline static constexpr char kUpsertTraceID[] = "UpsertTracepoint";
  inline static constexpr char kDeleteTracepointID[] = "DeleteTracepoint";
  inline static constexpr char kGoProbeTraceDefinition[] = "goprobe";

 protected:
  explicit TraceModule(DynamicTraceIR* probes, ASTVisitor* ast_visitor)
      : QLObject(TraceModuleType, ast_visitor), probes_(probes) {}
  Status Init();

 private:
  DynamicTraceIR* probes_;
};

class ProbeHandler {
 public:
  /**
   * @brief ProbeHandler is the handler for the @px.probe decorator. I find the structure of
   * decorators very confusing, but they are basically deeply nested functions. For
   * the probe() decorator., the equivalent function would look like this:
   * ```
   * def probe(fn_name, binary):
   *     def decorator_probes(func):
   *        def wrapper():
   *            pxtrace.StartProbe(fn_name, binary)
   *            r = func()
   *            pxtrace.EndProbe()
   *            # Returns the function return value.
   *            return r
   *        # Returns the wrapper function that will be called in place of func().
   *        return wrapper
   *    # Returns the decorator
   *    return decorator_probes
   * ```
   *
   * and then is called like the following
   * ```
   * @px.probe(...)
   * def probe_http():
   *    return [{"latency": pxtrace.FunctionLatency()},{"return": pxtrace.Return(0)}]
   * ```
   *
   * The AST Visitor will first call probes() with the arguments passed in then will
   *
   *
   * Whenever the decorator around a func, wrapper() replaces func() as the
   *
   * @param probes
   * @param ast
   * @param args
   * @param visitor
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Probe(
      DynamicTraceIR* probes, stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
      const pypa::AstPtr& ast, const ParsedArgs& args, ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> Decorator(
      DynamicTraceIR* probes, stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
      const std::string& function_name, const pypa::AstPtr& ast, const ParsedArgs& args,
      ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> Wrapper(
      DynamicTraceIR* probes, stirling::dynamic_tracing::ir::shared::BinarySpec::Language language,
      const std::string& function_name, const std::shared_ptr<FuncObject> func_obj,
      const pypa::AstPtr& ast, const ParsedArgs& args, ASTVisitor* visitor);
};

/**
 * @brief Implements the pxtrace.Argument() logic.
 *
 */
class ArgumentHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(DynamicTraceIR* probes, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
