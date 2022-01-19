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

#include <memory>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/perf_profiler/symbol_cache/symbol_cache.h"

namespace px {
namespace stirling {

constexpr int kAddr1 = 123;
constexpr int kAddr2 = 456;

std::string_view SymbolizationFn(const uintptr_t addr) {
  static std::string s;
  s = absl::Substitute("$0", addr);
  return s;
}

class SymbolCacheTest : public ::testing::Test {
 protected:
  void SetUp() override { sym_cache_ = std::make_unique<SymbolCache>(&SymbolizationFn); }

  std::unique_ptr<SymbolCache> sym_cache_;
};

TEST_F(SymbolCacheTest, Lookup) {
  SymbolCache::LookupResult result;

  result = sym_cache_->Lookup(kAddr1);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "123");

  result = sym_cache_->Lookup(kAddr1);
  EXPECT_EQ(result.hit, true);
  EXPECT_EQ(result.symbol, "123");

  result = sym_cache_->Lookup(kAddr2);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "456");
}

TEST_F(SymbolCacheTest, EvictOldEntries) {
  SymbolCache::LookupResult result;

  EXPECT_EQ(sym_cache_->total_entries(), 0);
  EXPECT_EQ(sym_cache_->active_entries(), 0);

  result = sym_cache_->Lookup(kAddr1);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "123");

  result = sym_cache_->Lookup(kAddr2);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "456");

  EXPECT_EQ(sym_cache_->total_entries(), 2);
  EXPECT_EQ(sym_cache_->active_entries(), 2);

  sym_cache_->PerformEvictions();

  EXPECT_EQ(sym_cache_->total_entries(), 2);
  EXPECT_EQ(sym_cache_->active_entries(), 0);

  result = sym_cache_->Lookup(kAddr1);
  EXPECT_EQ(result.hit, true);
  EXPECT_EQ(result.symbol, "123");

  EXPECT_EQ(sym_cache_->total_entries(), 2);
  EXPECT_EQ(sym_cache_->active_entries(), 1);

  sym_cache_->PerformEvictions();

  EXPECT_EQ(sym_cache_->total_entries(), 1);
  EXPECT_EQ(sym_cache_->active_entries(), 0);

  // Don't lookup test::foo() in this interval.
  // Should cause it to get evicted from the cache after the next trigger.

  sym_cache_->PerformEvictions();

  EXPECT_EQ(sym_cache_->total_entries(), 0);
  EXPECT_EQ(sym_cache_->active_entries(), 0);

  sym_cache_->PerformEvictions();

  EXPECT_EQ(sym_cache_->total_entries(), 0);
  EXPECT_EQ(sym_cache_->active_entries(), 0);

  result = sym_cache_->Lookup(kAddr1);
  EXPECT_EQ(result.hit, false);
  EXPECT_EQ(result.symbol, "123");

  EXPECT_EQ(sym_cache_->total_entries(), 1);
  EXPECT_EQ(sym_cache_->active_entries(), 1);
}

}  // namespace stirling
}  // namespace px
