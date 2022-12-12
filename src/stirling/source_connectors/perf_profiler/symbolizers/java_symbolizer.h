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
#include <vector>

#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/symbolizer.h"
#include "src/stirling/utils/monitor.h"

DECLARE_string(stirling_profiler_java_agent_libs);
DECLARE_string(stirling_profiler_px_jattach_path);
DECLARE_bool(stirling_profiler_java_symbols);
DECLARE_uint32(number_attach_attempts_per_iteration);

namespace px {
namespace stirling {

class JavaSymbolizationContext {
 public:
  struct SymbolAndCodeSize {
    std::string symbol;
    uint32_t size;
    SymbolAndCodeSize() {}
    SymbolAndCodeSize(const std::string sym, const uint32_t sz) : symbol(sym), size(sz) {}
  };
  using SymbolMapType = absl::btree_map<uint64_t, SymbolAndCodeSize>;

  JavaSymbolizationContext(const struct upid_t& target_upid,
                           profiler::SymbolizerFn native_symbolizer_fn,
                           std::unique_ptr<std::ifstream> symbol_file);
  ~JavaSymbolizationContext();

  std::string_view Symbolize(const uintptr_t addr);

  void RemoveArtifacts() const;

  void set_requires_refresh() { requires_refresh_ = true; }

 private:
  void UpdateSymbolMap();

  bool requires_refresh_ = false;
  SymbolMapType symbol_map_;
  profiler::SymbolizerFn native_symbolizer_fn_;
  std::unique_ptr<std::ifstream> symbol_file_;
  std::filesystem::path host_artifacts_path_;
};

class JavaSymbolizer : public Symbolizer {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create(
      std::unique_ptr<Symbolizer> native_symbolizer);

  profiler::SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;
  void IterationPreTick() override;
  void DeleteUPID(const struct upid_t& upid) override;
  bool Uncacheable(const struct upid_t& upid) override;

 private:
  JavaSymbolizer() = delete;
  explicit JavaSymbolizer(std::string&& agent_libs);
  Status CreateNewJavaSymbolizationContext(const struct upid_t& upid);
  std::string_view Symbolize(JavaSymbolizationContext* ctx, const uintptr_t addr);

  std::unique_ptr<Symbolizer> native_symbolizer_;
  absl::flat_hash_map<struct upid_t, profiler::SymbolizerFn> symbolizer_functions_;
  absl::flat_hash_map<struct upid_t, std::unique_ptr<java::AgentAttacher>> active_attachers_;
  absl::flat_hash_map<struct upid_t, std::unique_ptr<JavaSymbolizationContext>>
      symbolization_contexts_;
  const std::string agent_libs_;
  StirlingMonitor& monitor_ = *StirlingMonitor::GetInstance();
  uint32_t num_attaches_remaining_this_iteration_ = 0;
};

}  // namespace stirling
}  // namespace px
