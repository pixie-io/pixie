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

#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/probes/probes.h"

namespace px {
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
      /* name */ "TracingVariable",
      /* type */ QLObjectType::kTracingVariable,
  };

  static bool IsTracingVariable(const QLObjectPtr& ptr) {
    return ptr->type() == TracingVariableObjectType.type();
  }

  // The reference for this tracing variable.
  const std::string& id() const { return id_; }

  TracingVariableObject(const pypa::AstPtr& ast, ASTVisitor* visitor, std::string id)
      : QLObject(TracingVariableObjectType, ast, visitor), id_(std::move(id)) {}

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

  static StatusOr<std::shared_ptr<ProbeObject>> Create(const pypa::AstPtr& ast, ASTVisitor* visitor,
                                                       const std::shared_ptr<TracepointIR>& probe);

  static bool IsProbe(const QLObjectPtr& ptr) { return ptr->type() == ProbeObjectType.type(); }
  std::shared_ptr<TracepointIR> probe() const { return probe_; }

 private:
  ProbeObject(const pypa::AstPtr& ast, ASTVisitor* visitor, std::shared_ptr<TracepointIR> probe)
      : QLObject(ProbeObjectType, ast, visitor), probe_(std::move(probe)) {}

  std::shared_ptr<TracepointIR> probe_;
};

class TraceProgramObject : public QLObject {
 public:
  static constexpr TypeDescriptor TracePointProgramType = {
      /* name */ "TraceProgram",
      /* type */ QLObjectType::kTraceProgram,
  };

  static bool IsTraceProgram(const QLObjectPtr& ptr) {
    return ptr->type() == TracePointProgramType.type();
  }
  const std::string& program() const { return program_; }
  const std::vector<TracepointSelector>& selectors() const { return selectors_; }

  TraceProgramObject(const pypa::AstPtr& ast, ASTVisitor* visitor, const std::string& program,
                     const std::vector<TracepointSelector>& selectors)
      : QLObject(TracePointProgramType, ast, visitor),
        program_(std::move(program)),
        selectors_(selectors) {}

 private:
  std::string program_;
  std::vector<TracepointSelector> selectors_;
};

class TraceModule : public QLObject {
 public:
  static constexpr TypeDescriptor TraceModuleType = {
      /* name */ "pxtrace",
      /* type */ QLObjectType::kTraceModule,
  };
  static StatusOr<std::shared_ptr<TraceModule>> Create(MutationsIR* mutations_ir,
                                                       ASTVisitor* ast_visitor);

  // Constant for the modules.
  inline static constexpr char kTraceModuleObjName[] = "pxtrace";

  // Constants for functions of pxtrace.
  inline static constexpr char kArgExprID[] = "ArgExpr";
  inline static constexpr char kArgExprDocstring[] = R"doc(
  Specifies a function argument to trace.

  Extracts the function argument, as specified by the provided expression.
  Traceable types are base types (`int`, `float`, etc.), strings and byte arrays.
  Base-type arguments are specified directly (`arg1`), while struct members are
  accessed using dotted notation (`arg1.foo`). The dot operator works on both
  pointer and non-pointer types.

  :topic: tracepoint_fields

  Args:
    expr (str): The expression to evaluate.

  Returns:
    px.TracingField: A materialized column pointer to use in output table definitions.
  )doc";

  inline static constexpr char kRetExprID[] = "RetExpr";
  inline static constexpr char kRetExprDocstring[] = R"doc(
  Specifies a function return value to trace.

  Extracts data from the function return value, as specified by the provided expression.
  Traceable types are the same as in `ArgExpr`. Return values are accessed by index
  (`$0` for the first return value, `$1` for the second return value, etc.).
  In Golang, the first index value is the number of arguments, excluding the receiver.
  For example, the return value for `fun Sum(a int, b int) int` is `$2`.
  Return values that are structs may be accessed using dotted notation, similar to `ArgExpr`,
  (e.g. `$0.foo`).

  :topic: tracepoint_fields

  Args:
    expr (str): The expression to evaluate.

  Returns:
    px.TracingField: A materialized column pointer to use in output table definitions.
  )doc";

  inline static constexpr char kFunctionLatencyID[] = "FunctionLatency";
  inline static constexpr char kFunctionLatencyDocstring[] = R"doc(
  Specifies a function latency to trace.

  Computes the function latency, from entry to return. The measured latency includes
  includes time spent in sub-calls.

  :topic: tracepoint_fields

  Returns:
    px.TracingField: A materialized column pointer to use in output table definitions.
  )doc";

  inline static constexpr char kUpsertTraceID[] = "UpsertTracepoint";
  inline static constexpr char kUpsertTracepointDocstring[] = R"doc(
  Deploys a tracepoint on a process and collects the traced data into a table.

  Deploys the tracepoint on the process (UPID) for the specified amount of time (TTL).
  The provided name uniquely identifies the tracepoint, and is used to manage the
  tracepoint (e.g. future calls to `UpsertTracepoint` or `DeleteTracepoint`.)
  A call to `UpsertTracepoint` on an existing tracepoint resets the TTL, but
  otherwise has no effect. A call to `UpsertTracepoint` on an existing tracepoint
  with a different tracepoint function will fail. UpsertTracepoint automatically
  creates a table with the provided name should it not exist; if the table exists
  but has a different schema, the deployment will fail.

  :topic: pixie_state_management

  Args:
    name (str): The name of the tracepoint. Should be unique with the probe_fn.
    table_name (str): The table name to write the results. The table is created
      if it does not exist. The table schema must match if the table does exist.
    probe_fn (Union[px.ProbeFn, str, pxtrace.TraceProgram, List[pxtrace.TraceProgram]]): The tracepoint function, BPFTrace program or pxtrace.TraceProgram to deploy.
    target (Union[px.UPID,px.SharedObject,pxtrace.PodProcess,pxtrace.LabelSelector]): The process or shared object
      to trace as specified by unique Vizier PID.
    ttl (px.Duration): The length of time that a tracepoint will stay alive, after
      which it will be removed.
  )doc";

  inline static constexpr char kTraceProgramID[] = "TraceProgram";
  inline static constexpr char kTraceProgramDocstring[] = R"doc(
  Creates a trace program. Selectors for supported hosts can be specified using key-value arguments.

  :topic: pixie_state_management

  Args:
    program (str): The BPFtrace program string.
    min_kernel (str, optional): The minimum kernel version that the tracepoint is supported on. Format is `<version>.<major>.<minor>`.
    max_kernel (str, optional): The maximum kernel version that the tracepoint is supported on. Format is `<version>.<major>.<minor>`.
    host_name (str, optional): Restrict the tracepoint to a specific host.

  Returns:
    TraceProgram: A pointer to the TraceProgram that can be passed as a probe_fn
    to UpsertTracepoint.
  )doc";

  inline static constexpr char kDeleteTracepointID[] = "DeleteTracepoint";
  inline static constexpr char kDeleteTracepointDocstring[] = R"doc(
  Deletes a tracepoint.

  Deletes the tracepoint with the provided name, should it exist.

  :topic: pixie_state_management

  Args:
    name (str): The name of the tracepoint.
  )doc";

  inline static constexpr char kProbeTraceDefinition[] = "probe";
  inline static constexpr char kProbeDocstring[] = R"doc(
  Decorates a tracepoint definition.

  Specifies the decorated function as a tracepoint on the `trace_fn`
  name. Automatically figures out the language based on the functon
  specified.

  :topic: tracepoint_decorator

  Args:
    trace_fn (str): The func to trace. For go, the format is `<package_name>.<func_name>`.

  Returns:
    Func: The wrapped probe function.
  )doc";

  inline static constexpr char kSharedObjectID[] = "SharedObject";
  inline static constexpr char kSharedObjectDocstring[] = R"doc(
  Defines a shared object target for Tracepoints.

  :topic: tracepoint_fields

  Args:
    name (str): The name of the shared object.
    upid (px.UPID): A process which loads the shared object.

  Returns:
    SharedObject: A pointer to the SharedObject that can be passed as a target
    to UpsertTracepoint.
  )doc";

  inline static constexpr char kKProbeTargetID[] = "kprobe";
  inline static constexpr char kKProbeTargetDocstring[] = R"doc(
  Defines a kprobe target for an UpsertTracepoint.

  :topic: tracepoint_fields

  Returns:
    KProbeTarget: KProbe target that can be passed into UpsertTracepoint.
  )doc";

  inline static constexpr char kProcessTargetID[] = "PodProcess";
  inline static constexpr char kProcessTargetDocstring[] = R"doc(
  Creates a Tracepoint target for a process.

  Defines a tracepoint target for a process based on the pod and if that's not specific
  enough a container and process path.

  :topic: tracepoint_fields

  Args:
    pod_name (str): The name of the pod running the target process. Must be of the format <namespace>/<pod>.
      You may also use the prefix of the pod name to avoid writing the kubernetes generated check-sum.
    container_name (str, optional): The name of the container that's running the process.
      Specify this argument if a pod has more than one containers. The compiler will error out
      if a pod has multiple containers and this is not specified.
    process_name (str, optional): A regexp that matches any substrings of the command line of
      the target process. Specify this if a container has more than one process. The compiler will
      error out if a container has multiple processes and this is not specified.

  Returns:
    ProcessTarget: A pointer to that Process that can be passed as a target
    to UpsertTracepoint.
  )doc";

  inline static constexpr char kLabelSelectorTargetID[] = "LabelSelector";
  inline static constexpr char kLabelSelectorTargetDocString[] = R"doc(
  Creates a Tracepoint target for a process.

  Defines a tracepoint target for a process based a set of k8s labels representing a set of pods matching all
  of this labels. Optionally, a container and process can be provided.

  :topic: tracepoint_fields

  Args:
    labels (dict<str, str>): The K8s labels that can be resolved to a set of pods. Must be of a dictionary of key value pairs.
    namespace (str): The namespace that target pods are in.
    container_name (str, optional): The name of the container that's running the process.
      Specify this argument if a pod has more than one containers. The compiler will error out
      if a pod has multiple containers and this is not specified.
    process_name (str, optional): A regexp that matches any substrings of the command line of
      the target process. Specify this if a container has more than one process. The compiler will
      error out if a container has multiple processes and this is not specified.

  Returns:
    ProcessTarget: A pointer to that Process that can be passed as a target
    to UpsertTracepoint.
  )doc";

 protected:
  explicit TraceModule(MutationsIR* mutations_ir, ASTVisitor* ast_visitor)
      : QLObject(TraceModuleType, ast_visitor), mutations_ir_(mutations_ir) {}
  Status Init();

 private:
  MutationsIR* mutations_ir_;
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
   * @param mutations_ir
   * @param ast
   * @param args
   * @param visitor
   * @return StatusOr<QLObjectPtr>
   */
  static StatusOr<QLObjectPtr> Probe(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                     const ParsedArgs& args, ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> Decorator(MutationsIR* mutations_ir,
                                         const std::string& function_name, const pypa::AstPtr& ast,
                                         const ParsedArgs& args, ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> Wrapper(MutationsIR* mutations_ir, const std::string& function_name,
                                       const std::shared_ptr<FuncObject> func_obj,
                                       const pypa::AstPtr& ast, const ParsedArgs& args,
                                       ASTVisitor* visitor);
};

/**
 * @brief Implements the pxtrace.Argument() logic.
 *
 */
class ArgumentHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(MutationsIR* mutations_ir, const pypa::AstPtr& ast,
                                    const ParsedArgs& args, ASTVisitor* visitor);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
