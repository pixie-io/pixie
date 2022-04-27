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

#include "src/stirling/bpf_tools/bcc_symbolizer.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/perf_profiler/shared/types.h"
#include "src/stirling/source_connectors/perf_profiler/symbol_cache/symbol_cache.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

/**
 * Symbolizer: provides an API to resolve a program address to a symbol.
 *
 * A typical use case looks like this:
 *   auto symbolize_fn = symbolizer.GetSymbolizerFn(upid);
 *   const std::string symbol = symbolize_fn(addr);
 */
class Symbolizer : public NotCopyable {
 public:
  virtual ~Symbolizer() = default;

  /**
   * Create a symbolizer for the process specified by UPID.
   * The returned symbolizer function converts addresses to symbols for the process.
   */
  virtual profiler::SymbolizerFn GetSymbolizerFn(const struct upid_t& upid) = 0;

  /**
   * Performs any preprocessing that should happen per iteration on this Symbolizer.
   */
  virtual void IterationPreTick() = 0;

  /**
   * Delete the state associated with a symbolizer created by a previous call to GetSymbolizerFn
   */
  virtual void DeleteUPID(const struct upid_t& upid) = 0;

  /**
   * Indicates that underlying symbols cannot be cached because they are subject to change.
   */
  virtual bool Uncacheable(const struct upid_t& upid) = 0;
};

}  // namespace stirling
}  // namespace px
