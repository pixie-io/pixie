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

#include "src/stirling/source_connectors/perf_profiler/symbolizers/symbolizer.h"

namespace px {
namespace stirling {

/**
 * A Symbolizer that uses BCC's symbolizer under the hood.
 */
class BCCSymbolizer : public Symbolizer, public NotCopyMoveable {
 public:
  static StatusOr<std::unique_ptr<Symbolizer>> Create();

  profiler::SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) override;
  void IterationPreTick() override {}
  void DeleteUPID(const struct upid_t& upid) override;
  bool Uncacheable(const struct upid_t& /*upid*/) override { return false; }

 private:
  BCCSymbolizer() = default;
  bpf_tools::BCCSymbolizer bcc_symbolizer_;
};

}  // namespace stirling
}  // namespace px
