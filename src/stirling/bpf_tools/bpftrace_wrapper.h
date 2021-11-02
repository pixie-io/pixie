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

#include <bpftrace.h>
#include <driver.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace bpf_tools {

// Dump the bpftrace program's syntax data and other relevant internal process data.
std::string DumpDriver(const bpftrace::Driver& driver);

/**
 * Wrapper around BPFTrace, as a convenience.
 */
class BPFTraceWrapper {
 public:
  /**
   * Callback from BPFTrace is a raw pointer to data which needs to be
   * interpreted as the printf data.
   */
  using PrintfCallback = std::function<void(uint8_t*)>;

  BPFTraceWrapper();

  ~BPFTraceWrapper() { Stop(); }

  /**
   * Compiles the BPFTrace program, assuming the output mode is via printfs.
   *
   * @return error if there is a syntax or semantic error.
   *         Also errors if there is no printf statement in the compiled program,
   *         or if all the printfs in the program do not have a consistent format string.
   */
  Status CompileForPrintfOutput(std::string_view script,
                                const std::vector<std::string>& params = {});

  /**
   * Compiles the BPFTrace program, but without treating printfs in any special way.
   * Meant for model where output is through BPF maps.
   *
   * @return error if there is a syntax or semantic error.
   */
  Status CompileForMapOutput(std::string_view script, const std::vector<std::string>& params = {});

  /**
   * Deploys the previously compiled BPFTrace program.
   */
  Status Deploy(const PrintfCallback& printf_callback = nullptr);

  /**
   * Drains all of the managed perf buffers, calling the handle function for each event.
   */
  void PollPerfBuffers(int timeout_ms = 100);

  /**
   * Stops the running BPFTrace program.
   */
  void Stop();

  /**
   * Returns the fields of the BPFTrace program's printf.
   * Only valid if compiled with printf_to_table option; undefined behavior otherwise.
   */
  const std::vector<bpftrace::Field>& OutputFields() const;

  /**
   * Returns the format string of the BPFTrace program's printf.
   * Only valid if compiled with printf_to_table option, undefined behavior otherwise.
   */
  std::string_view OutputFmtStr() const;

  /**
   * Gets the specified map name, using BPFTrace name. Map name should include the '@' prefix.
   */
  bpftrace::BPFTraceMap GetBPFMap(const std::string& name);

  /**
   * Get direct access to the BPFTrace object.
   */
  bpftrace::BPFtrace* mutable_bpftrace() { return &bpftrace_; }

 private:
  Status Compile(std::string_view script, const std::vector<std::string>& params);

  // Checks the output for dynamic tracing:
  //  1) There must be at least one printf.
  //  2) If there is more than one printf, all printfs have a consistent format string.
  //     In other words, all printfs must output to the same table.
  Status CheckPrintfs() const;

  // The primary BPFtrace object.
  bpftrace::BPFtrace bpftrace_;

  // The compiled BPF bytecode.
  bpftrace::BpfBytecode bytecode_;

  std::mutex compilation_mutex_;

  bool compiled_ = false;
  bool printf_to_table_ = false;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
