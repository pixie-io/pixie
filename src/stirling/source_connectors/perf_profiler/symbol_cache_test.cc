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

#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/perf_profiler/symbol_cache.h"

namespace test {
// foo() & bar() are not used directly, but in this test,
// we will find their symbols using the device under test, the symbolizer.
void foo() { LOG(INFO) << "foo()."; }
void bar() { LOG(INFO) << "bar()."; }
}  // namespace test

const uintptr_t kFooAddr = reinterpret_cast<uintptr_t>(&test::foo);
const uintptr_t kBarAddr = reinterpret_cast<uintptr_t>(&test::bar);

namespace px {
namespace stirling {

class SymbolCacheTest : public ::testing::Test {
 protected:
  bpf_tools::BCCSymbolizer bcc_symbolizer_;
};

TEST_F(SymbolCacheTest, Lookup) {
  SymbolCache sym_cache(getpid(), &bcc_symbolizer_);

  SymbolCache::LookupResult result;

  result = sym_cache.Lookup(kFooAddr);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "test::foo()");

  result = sym_cache.Lookup(kFooAddr);
  EXPECT_EQ(result.hit, true);
  EXPECT_EQ(result.symbol, "test::foo()");

  result = sym_cache.Lookup(kBarAddr);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "test::bar()");
}

TEST_F(SymbolCacheTest, EvictOldEntries) {
  SymbolCache sym_cache(getpid(), &bcc_symbolizer_);

  SymbolCache::LookupResult result;

  EXPECT_EQ(sym_cache.total_entries(), 0);
  EXPECT_EQ(sym_cache.active_entries(), 0);

  result = sym_cache.Lookup(kFooAddr);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "test::foo()");

  result = sym_cache.Lookup(kBarAddr);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "test::bar()");

  EXPECT_EQ(sym_cache.total_entries(), 2);
  EXPECT_EQ(sym_cache.active_entries(), 2);

  sym_cache.CreateNewGeneration();

  EXPECT_EQ(sym_cache.total_entries(), 2);
  EXPECT_EQ(sym_cache.active_entries(), 0);

  result = sym_cache.Lookup(kFooAddr);
  EXPECT_EQ(result.hit, true);
  EXPECT_EQ(result.symbol, "test::foo()");

  EXPECT_EQ(sym_cache.total_entries(), 2);
  EXPECT_EQ(sym_cache.active_entries(), 1);

  sym_cache.CreateNewGeneration();

  EXPECT_EQ(sym_cache.total_entries(), 1);
  EXPECT_EQ(sym_cache.active_entries(), 0);

  // Don't lookup test::foo() in this interval.
  // Should cause it to get evicted from the cache after the next trigger.

  sym_cache.CreateNewGeneration();

  EXPECT_EQ(sym_cache.total_entries(), 0);
  EXPECT_EQ(sym_cache.active_entries(), 0);

  sym_cache.CreateNewGeneration();

  EXPECT_EQ(sym_cache.total_entries(), 0);
  EXPECT_EQ(sym_cache.active_entries(), 0);

  result = sym_cache.Lookup(kFooAddr);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "test::foo()");

  EXPECT_EQ(sym_cache.total_entries(), 1);
  EXPECT_EQ(sym_cache.active_entries(), 1);
}

}  // namespace stirling
}  // namespace px
