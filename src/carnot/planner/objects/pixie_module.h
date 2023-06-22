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
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class PixieModule : public QLObject {
 public:
  static constexpr TypeDescriptor PixieModuleType = {
      /* name */ "px",
      /* type */ QLObjectType::kPLModule,
  };
  static StatusOr<std::shared_ptr<PixieModule>> Create(
      IR* graph, CompilerState* compiler_state, ASTVisitor* ast_visitor,
      bool func_based_exec = false, const absl::flat_hash_set<std::string>& reserved_names = {});

  // Constant for the modules.
  inline static constexpr char kPixieModuleObjName[] = "px";
  inline static constexpr char kOldPixieModuleObjName[] = "pl";

  // Operator function names.
  inline static constexpr char kDataframeOpID[] = "DataFrame";
  inline static constexpr char kDisplayOpID[] = "display";
  inline static constexpr char kDisplayOpDocstring[] = R"doc(
  Outputs the data from the engine.

  Writes the data to the output stream. Disabled if executing using Vis functions.
  If you want to still see data when using vis fucntions, use `px.debug`.

  Examples:
    px.display(df, 'http_data')

  :topic: dataframe_ops

  Args:
    out (px.DataFrame): The DataFrame to write out to the output stream.
    name (string): The output table name for the DataFrame. If not set, then
      will be 'output'. If the name is duplicated across all written tables, we
      suffix with `_1`, incrementing for every duplicate.
  )doc";

  inline static constexpr char kExportOpID[] = "export";
  inline static constexpr char kExportOpDocstring[] = R"doc(
  Sends a DataFrame from Pixie to the specified output.

  Writes the data to the specified output destination. For example, can be
  used to specify an export to OpenTelemetry using the methods available in
  `px.otel.trace` or `px.otel.metrics`.

  Examples:
    px.export(df, px.otel.Data(...))

  :topic: dataframe_ops

  Args:
    out (px.DataFrame): The DataFrame to write out to the output stream.
    export_spec (Exporter): The destination specification for the DataFrame data.

  )doc";

  inline static constexpr char kDebugTablePrefix[] = "_";
  inline static constexpr char kDebugOpID[] = "debug";
  inline static constexpr char kDebugOpDocstring[] = R"doc(
  Outputs the data from the engine as a debug table

  Writes the data to the output stream, prefixing the name with `_`. Unlike `px.display`
  if executing the script with Vis functions, this will still write to the output table.
  Debug tables are displayed in the data drawer in the Live UI. To show / hide the data
  drawer use cmd+d (Mac) or ctrl+d (Windows, Linux).

  Examples:
    px.debug(df, 'test_data')

  :topic: dataframe_ops

  Args:
    out (px.DataFrame): The DataFrame to write out to the output stream.
    name (string): The output table name for the DataFrame. If not set, then
      will be 'output'. If the name is duplicated across all written tables, we
      suffix with `_1`, incrementing for every duplicate.
  )doc";

  // Compile time functions
  inline static constexpr char kNowOpID[] = "now";
  inline static constexpr char kNowOpDocstring[] = R"doc(
  Get the current time.

  :topic: compile_time_fn

  Returns:
    px.Time: The current time as defined at the start of compilation.

  )doc";
  inline static constexpr char kEqualsAnyID[] = "equals_any";
  inline static constexpr char kEqualsAnyDocstring[] = R"doc(
  Returns true if the value is in the list.

  Check equality of the input value with every element of a list.

  Examples:
    df.val = px.equals_any(df.remote_addr, ['10.0.0.1', '10.0.0.2'])

  :topic: compile_time_fn

  Args:
    value (px.Expr): The value to compare.
    comparisons (List[px.Expr]): The list of values to check equality to the value.

  Returns:
    px.Expr: An expression that evaluates to true if the value is found in the list.

  )doc";

  inline static constexpr char kUInt128ConversionID[] = "uint128";
  inline static constexpr char kUInt128ConversionDocstring[] = R"doc(
  Parse the UUID string into a UInt128.

  Parse the UUID string of canonical textual representation into a 128bit
  integer (ie "123e4567-e89b-12d3-a456-426614174000"). Errors out if the string
  is not the correct format.

  Examples:
    val = px.uint128("123e4567-e89b-12d3-a456-426614174000")

  :topic: compile_time_fn

  Args:
    uuid (string): the uuid in canoncial uuid4 format ("123e4567-e89b-12d3-a456-426614174000")
  Returns:
    uint128: The uuid as a uint128.
  )doc";
  inline static constexpr char kMakeUPIDID[] = "make_upid";
  inline static constexpr char kMakeUPIDDocstring[] = R"doc(
  Create a UPID from its components to represent a process.

  Creates a UPID object from asid, pid, and time started in nanoseconds. UPID stands for
  unique PID and is a Pixie concept to ensure tracked processes are unique in time and across
  nodes.

  Note: Creating this value from scratch might be very difficult, especially given the nanosecond timestamp.
  It's probably only useful if you find the UPID printed out as it's constituent components.

  In most situations, you might find that `px.uint128` is a better option as we often render UPID as uuid.

  Examples:
    val = px.make_upid(123, 456, 1598419816000000)

  :topic: compile_time_fn

  Args:
    asid (int): The ID of the node, according to the Pixie metadata service. Stands for Agent short ID.
    pid (int): The PID of the process on the node.
    ts_ns (int): The start time of the process in unix time.
  Returns:
    px.UPID: The represented UPID.
  )doc";
  inline static constexpr char kAbsTimeOpID[] = "strptime";
  inline static constexpr char kAbsTimeDocstring[] = R"doc(
  Parse a datestring into a px.Time.

  Parse a datestring using a standard time format template into an internal time representation.
  The format must follow the C strptime format, outlined in this document:
  https://pubs.opengroup.org/onlinepubs/009695399/functions/strptime.html

  Examples:
    time = px.strptime("2020-03-12 19:39:59 -0200", "%Y-%m-%d %H:%M:%S %z")

  :topic: compile_time_fn

  Args:
    date_string (string): The time as a string, should match the format object.
    format (string): The string format according to the C strptime format
      https://pubs.opengroup.org/onlinepubs/009695399/functions/strptime.html
  Returns:
    px.Time: The time value represented in the data.
  )doc";

  inline static constexpr char kFormatDurationOpID[] = "format_duration";
  inline static constexpr char kFormatDurationDocstring[] = R"doc(
  Convert a duration in nanoseconds to a duration string.

  Convert an integer in nanoseconds (-3,000,000,000) to a duration string ("-5m") while
  preserving the sign. This function converts to whole milliseconds, seconds, minutes,
  hours or days. This means it will round down to the nearest whole number time unit.

  Examples:
    # duration = "-5m"
    duration = px.format_duration(-5 * 60 * 1000 * 1000 * 1000)
    # duration = "5m" duration is rounded down to nearest whole minute.
    duration = px.parse_duration(-5 * 60 * 1000 * 1000 * 1000 + 5)
    # duration = "-5h"
    duration = px.format_duration(-5 * 60 * 60 * 1000 * 1000 * 1000)

  :topic: compile_time_fn

  Args:
    duration (int64): The duration in nanoseconds.
  Returns:
    string: The string representing the human readable duration (i.e. -5m, -10h).
  )doc";
  inline static constexpr char kParseDurationOpID[] = "parse_duration";
  inline static constexpr char kParseDurationDocstring[] = R"doc(
  Parse a duration string to a duration in nanoseconds.

  Parse a duration string ("-5m") to integer nanoseconds (-3,000,000,000) while preserving
  the sign from the string.

  Examples:
    # duration = -300000000000
    duration = px.parse_duration("-5m")
    # duration = 300000000000
    duration = px.parse_duration("5m")

  :topic: compile_time_fn

  Args:
    duration (string): The duration in string form.
  Returns:
    px.Duration: The duration in nanoseconds.
  )doc";
  inline static constexpr char kParseTimeOpID[] = "parse_time";
  inline static constexpr char kParseTimeDocstring[] = R"doc(
  Parse the various time formats into a unified format.

  This function can unify all possible time formats into a single consistent type.
  Useful for doing calculations on these values in a way that is agnostic to their format.

  Examples:
    ## As a relative time string.
    # time = now -300000000000
    time = px.parse_time("-5m")
    # time = now + 300000000000
    time = px.parse_time("5m")
    # Takes in px.Time
    time = px.parse_time(px.strptime("2020-03-12 19:39:59 -0200", "%Y-%m-%d %H:%M:%S %z"))
    # Takes in int
    time = px.parse_time(10)

  :topic: compile_time_fn

  Args:
    time (string, int, time): The time as a string, int, or px.Time.
  Returns:
    px.Time: The unix timestamp in nanoseconds.
  )doc";

  inline static constexpr char kScriptReferenceID[] = "script_reference";
  inline static constexpr char kScriptReferenceDocstring[] = R"doc(
  Create a reference to a PxL script.

  Create a reference to a PxL script with specified script arguments.
  These values are displayed in the UI as a clickable link to execute that PxL script.

  Examples:
    df.script = px.script_reference(df.namespace, 'px/namespace', {'namespace': df.namespace, 'start_time': '-5m'})

  :topic: compile_time_fn

  Args:
    label (string): A value containing the label text for the output deep link.
    script (string): The script ID to execute, such as 'px/namespace'.
    args (dictionary): A dictionary containing the script argument values.

  Returns:
    string: A stringified JSON representing the script, shown in the UI as a link.

  )doc";

  inline static constexpr char kTimeFuncDocstringTpl[] = R"doc(
  Gets the specified number of $0.

  Examples:
    # Returns 2 $0.
    time = px.$0(2)

  :topic: compile_time_fn

  Args:
    unit (int): The number of $0 to render.
  Returns:
    px.Duration: Duration representing `unit` $0.
  )doc";
  static constexpr std::array<std::pair<const char*, std::chrono::nanoseconds>, 6> kTimeFuncValues =
      {{{"minutes", std::chrono::minutes(1)},
        {"hours", std::chrono::hours(1)},
        {"seconds", std::chrono::seconds(1)},
        {"days", std::chrono::hours(24)},
        {"microseconds", std::chrono::microseconds(1)},
        {"milliseconds", std::chrono::milliseconds(1)}}};

  // Type constants.
  inline static constexpr char kTimeTypeName[] = "Time";
  inline static constexpr char kContainerTypeName[] = "Container";
  inline static constexpr char kNamespaceTypeName[] = "Namespace";
  inline static constexpr char kNodeTypeName[] = "Node";
  inline static constexpr char kPodTypeName[] = "Pod";
  inline static constexpr char kServiceTypeName[] = "Service";
  inline static constexpr char kBytesTypeName[] = "Bytes";
  inline static constexpr char kDurationNSTypeName[] = "DurationNanos";
  inline static constexpr char kUPIDTypeName[] = "UPID";
  inline static constexpr char kPercentTypeName[] = "Percent";

  // Submodules of Px.
  inline static constexpr char kVisAttrID[] = "vis";

 protected:
  explicit PixieModule(IR* graph, CompilerState* compiler_state, ASTVisitor* ast_visitor,
                       bool func_based_exec, const absl::flat_hash_set<std::string>& reserved_names)
      : QLObject(PixieModuleType, ast_visitor),
        graph_(graph),
        compiler_state_(compiler_state),
        func_based_exec_(func_based_exec),
        reserved_names_(reserved_names) {}
  Status Init();
  Status RegisterUDFFuncs();
  Status RegisterUDTFs();
  Status RegisterCompileTimeFuncs();
  Status RegisterTypeObjs();

 private:
  IR* graph_;
  CompilerState* compiler_state_;
  absl::flat_hash_set<std::string> compiler_time_fns_;
  const bool func_based_exec_;
  absl::flat_hash_set<std::string> reserved_names_;
};

// Helper function to add a result sink to an IR.
inline Status AddResultSink(IR* graph, const pypa::AstPtr& ast, std::string_view out_name,
                            OperatorIR* parent_op, std::string_view result_addr,
                            std::string_view result_ssl_targetname) {
  // It's a bit more concise to do column selection using a keep:
  // px.display(df[['cols', 'to', 'keep']])
  // than passing cols as a separate param:
  // px.display(df, cols=['cols', 'to', 'keep'])
  // So we don't currently support passing those output columns as an argument to display.
  std::vector<std::string> columns;
  PX_ASSIGN_OR_RETURN(
      auto sink, graph->CreateNode<GRPCSinkIR>(ast, parent_op, std::string(out_name), columns));
  sink->SetDestinationAddress(std::string(result_addr));
  sink->SetDestinationSSLTargetName(std::string(result_ssl_targetname));
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
