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

#include <set>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/bcc_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/caching_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/elf_symbolizer.h"
#include "src/stirling/testing/symbolization.h"

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

template <typename TSymbolizer>
class SymbolizerTest : public ::testing::Test {
 public:
  SymbolizerTest() = default;
  void SetUp() override { ASSERT_OK_AND_ASSIGN(symbolizer_, TSymbolizer::Create()); }
  std::unique_ptr<Symbolizer> symbolizer_;
};

using ElfSymbolizerTest = SymbolizerTest<ElfSymbolizer>;
using BCCSymbolizerTest = SymbolizerTest<BCCSymbolizer>;

using SymbolizerTypes = ::testing::Types<ElfSymbolizer, BCCSymbolizer>;
TYPED_TEST_SUITE(SymbolizerTest, SymbolizerTypes);

TYPED_TEST(SymbolizerTest, UserSymbols) {
  // We will use our self pid for symbolizing symbols from within this process.
  struct upid_t this_upid;
  this_upid.pid = static_cast<uint32_t>(getpid());
  this_upid.start_time_ticks = 0;

  auto symbolize = this->symbolizer_->GetSymbolizerFn(this_upid);

  EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
  EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
  EXPECT_EQ(symbolize(2), std::string("0x0000000000000002"));
}

TEST_F(BCCSymbolizerTest, KernelSymbols) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer, BCCSymbolizer::Create());

  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(const uint64_t kaddr, GetKernelSymAddr(kSymbolName));

  auto symbolize = symbolizer_->GetSymbolizerFn(profiler::kKernelUPID);
  EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
}

// Test the symbolizer with caching enabled and disabled.
TEST_F(BCCSymbolizerTest, Caching) {
  // We will use our self pid for symbolizing symbols from within this process,
  // *and* we will trigger the kprobe that grabs a symbol from the kernel.
  const uint32_t pid = getpid();

  // Create a caching layer around our symbolizer.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer_uptr,
                       CachingSymbolizer::Create(std::move(symbolizer_)));
  CachingSymbolizer& symbolizer = *static_cast<CachingSymbolizer*>(symbolizer_uptr.get());

  const struct upid_t this_upid = {.pid = pid, .start_time_ticks = 0};

  // Lookup the addresses for the first time. These should be cache misses.
  // We are placing each symbol lookup into its own scope to force us to
  // "re-lookup" the pid symbolizer function from inside of the symbolize instance.
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 1);
    EXPECT_EQ(symbolizer.stat_hits(), 0);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 1);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 2);
    EXPECT_EQ(symbolizer.stat_hits(), 0);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }

  // Lookup the addresses a second time. We should get cache hits.
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 3);
    EXPECT_EQ(symbolizer.stat_hits(), 1);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }

  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 4);
    EXPECT_EQ(symbolizer.stat_hits(), 2);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }

  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(const uint64_t kaddr, GetKernelSymAddr(kSymbolName));

  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 5);
    EXPECT_EQ(symbolizer.stat_hits(), 2);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 6);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }

  // This will flush the caches, access count & hit count will remain the same.
  // We will lookup the symbols and again expect a miss then a hit.
  symbolizer.DeleteUPID(this_upid);
  EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 1);
  symbolizer.DeleteUPID(profiler::kKernelUPID);
  EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 0);

  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 7);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 1);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 8);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 9);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 10);
    EXPECT_EQ(symbolizer.stat_hits(), 4);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 11);
    EXPECT_EQ(symbolizer.stat_hits(), 5);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 12);
    EXPECT_EQ(symbolizer.stat_hits(), 6);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }

  // Test the feature that converts "[UNKNOWN]" into 0x<addr>.
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(0x1234123412341234ULL), "0x1234123412341234");
    EXPECT_EQ(symbolizer.stat_accesses(), 13);
    EXPECT_EQ(symbolizer.stat_hits(), 6);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 4);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(0x1234123412341234ULL), "0x1234123412341234");
    EXPECT_EQ(symbolizer.stat_accesses(), 14);
    EXPECT_EQ(symbolizer.stat_hits(), 7);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 4);
  }
}

}  // namespace stirling
}  // namespace px
