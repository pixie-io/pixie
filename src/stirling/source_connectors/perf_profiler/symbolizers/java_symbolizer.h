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

#include "src/stirling/source_connectors/perf_profiler/symbolizers/symbolizer.h"

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

  JavaSymbolizationContext(profiler::SymbolizerFn native_symbolizer_fn,
                           std::unique_ptr<std::ifstream> symbol_file);
  ~JavaSymbolizationContext();

  std::string_view Symbolize(const uintptr_t addr);

  void set_requires_refresh() { requires_refresh_ = true; }

 private:
  void UpdateSymbolMap();

  bool requires_refresh_ = false;
  SymbolMapType symbol_map_;
  profiler::SymbolizerFn native_symbolizer_fn_;
  std::unique_ptr<std::ifstream> symbol_file_;
};

class JavaSymbolizer : public Symbolizer {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create(
      std::unique_ptr<Symbolizer> native_symbolizer);

  profiler::SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;
  void IterationPreTick() override;
  void DeleteUPID(const struct upid_t& upid) override;

 private:
  JavaSymbolizer() = delete;
  explicit JavaSymbolizer(const std::vector<std::filesystem::path> agent_libs);
  std::string_view Symbolize(JavaSymbolizationContext* ctx, const uintptr_t addr);

  std::filesystem::path GetAgentSymbolFilePathPfx(const struct upid_t& pid) const;
  std::filesystem::path GetStirlingSymbolFilePath(const struct upid_t& pid) const;

  std::unique_ptr<Symbolizer> native_symbolizer_;
  absl::flat_hash_map<struct upid_t, profiler::SymbolizerFn> symbolizer_functions_;
  absl::flat_hash_map<struct upid_t, std::unique_ptr<JavaSymbolizationContext>>
      symbolization_contexts_;
  const std::vector<std::filesystem::path> agent_libs_;
};

}  // namespace stirling
}  // namespace px
