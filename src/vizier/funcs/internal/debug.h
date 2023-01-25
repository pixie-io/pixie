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
#include <string>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/common/perf/tcmalloc.h"
#include "src/common/system/proc_parser.h"
#include "src/shared/types/typespb/types.pb.h"

#ifdef TCMALLOC
#include <gperftools/malloc_extension.h>
#endif

namespace px {
namespace vizier {
namespace funcs {

constexpr int kMaxBufferSize = 1024 * 1024;

class HeapStatsUDTF final : public carnot::udf::UDTF<HeapStatsUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("heap", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pretty heap stats"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    std::string buf(kMaxBufferSize, '\0');
    MallocExtension::instance()->GetStats(&buf[0], buf.size());
    buf.resize(strlen(buf.c_str()));

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>(buf);
#else
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>("Only supported with tcmalloc.");
#endif

    return false;
  }
};

class HeapSampleUDTF final : public carnot::udf::UDTF<HeapSampleUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("heap", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pretty heap stats"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    std::string buf;
    MallocExtension::instance()->GetHeapSample(&buf);

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>(buf);
#else
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>("Only supported with tcmalloc.");
#endif

    return false;
  }
};

class HeapGrowthStacksUDTF final : public carnot::udf::UDTF<HeapGrowthStacksUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("heap", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pretty heap stats"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    std::string buf;
    MallocExtension::instance()->GetHeapGrowthStacks(&buf);

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>(buf);
#else
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("heap")>("Only supported with tcmalloc.");
#endif

    return false;
  }
};

class AgentProcStatusUDTF final : public carnot::udf::UDTF<AgentProcStatusUDTF> {
 public:
  using Config = system::Config;
  using ProcParser = system::ProcParser;
  static constexpr auto Executor() {
    // Kelvins don't mount the host filesystem and so cannot access /proc files.
    // If we change kelvins to allow access to the host filesystem, this UDTF could target
    // all agents.
    return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_PEM;
  }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                "The short ID of the agent", types::SemanticType::ST_ASID),
        ColInfo("vm_peak_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "peak virtual memory size", types::SemanticType::ST_BYTES),
        ColInfo("vm_size_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "total program size", types::SemanticType::ST_BYTES),
        ColInfo("vm_lck_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "locked memory size", types::SemanticType::ST_BYTES),
        ColInfo("vm_pin_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "pinned memory size", types::SemanticType::ST_BYTES),
        ColInfo("vm_hwm_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "peak resident set size (high water mark)", types::SemanticType::ST_BYTES),
        ColInfo("vm_rss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of memory portions (vm_rss = rss_anon + rss_file + rss_shmem)",
                types::SemanticType::ST_BYTES),
        ColInfo("rss_anon_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of resident anonymous memory", types::SemanticType::ST_BYTES),
        ColInfo("rss_file_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of resident file mappings", types::SemanticType::ST_BYTES),
        ColInfo("rss_shmem_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of resident shmem memory", types::SemanticType::ST_BYTES),
        ColInfo("vm_data_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of private data segments", types::SemanticType::ST_BYTES),
        ColInfo("vm_stk_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of stack segments", types::SemanticType::ST_BYTES),
        ColInfo("vm_exe_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of text segment", types::SemanticType::ST_BYTES),
        ColInfo("vm_lib_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of shared library code", types::SemanticType::ST_BYTES),
        ColInfo("vm_pte_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of page table entries", types::SemanticType::ST_BYTES),
        ColInfo("vm_swap_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "amount of swap used by anonymous private data", types::SemanticType::ST_BYTES),
        ColInfo("hugetlb_pages_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of hugetlb memory portions", types::SemanticType::ST_BYTES),
        ColInfo("voluntary_ctxt_switches", types::DataType::INT64,
                types::PatternType::METRIC_COUNTER, "number of voluntary context switches"),
        ColInfo("nonvoluntary_ctxt_switches", types::DataType::INT64,
                types::PatternType::METRIC_COUNTER, "number of non voluntary context switches"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    const ProcParser proc_parser;

    ProcParser::ProcessStatus stats;
    auto status = proc_parser.ParseProcPIDStatus(ctx->metadata_state()->pid(), &stats);
    if (!status.ok()) {
      LOG(ERROR) << "/proc/<pid>/status collection failed" << std::endl;
      return false;
    }
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("vm_peak_bytes")>(stats.vm_peak_bytes);
    rw->Append<IndexOf("vm_size_bytes")>(stats.vm_size_bytes);
    rw->Append<IndexOf("vm_lck_bytes")>(stats.vm_lck_bytes);
    rw->Append<IndexOf("vm_pin_bytes")>(stats.vm_pin_bytes);
    rw->Append<IndexOf("vm_hwm_bytes")>(stats.vm_hwm_bytes);
    rw->Append<IndexOf("vm_rss_bytes")>(stats.vm_rss_bytes);
    rw->Append<IndexOf("rss_anon_bytes")>(stats.rss_anon_bytes);
    rw->Append<IndexOf("rss_file_bytes")>(stats.rss_file_bytes);
    rw->Append<IndexOf("rss_shmem_bytes")>(stats.rss_shmem_bytes);
    rw->Append<IndexOf("vm_data_bytes")>(stats.vm_data_bytes);
    rw->Append<IndexOf("vm_stk_bytes")>(stats.vm_stk_bytes);
    rw->Append<IndexOf("vm_exe_bytes")>(stats.vm_exe_bytes);
    rw->Append<IndexOf("vm_lib_bytes")>(stats.vm_lib_bytes);
    rw->Append<IndexOf("vm_pte_bytes")>(stats.vm_pte_bytes);
    rw->Append<IndexOf("vm_swap_bytes")>(stats.vm_swap_bytes);
    rw->Append<IndexOf("hugetlb_pages_bytes")>(stats.hugetlb_pages_bytes);
    rw->Append<IndexOf("voluntary_ctxt_switches")>(stats.voluntary_ctxt_switches);
    rw->Append<IndexOf("nonvoluntary_ctxt_switches")>(stats.nonvoluntary_ctxt_switches);

    return false;
  }
};

class AgentProcSMapsUDTF final : public carnot::udf::UDTF<AgentProcSMapsUDTF> {
 public:
  using Config = system::Config;
  using ProcParser = system::ProcParser;
  static constexpr auto Executor() {
    // Kelvins don't mount the host filesystem and so cannot access /proc files.
    // If we change kelvins to allow access to the host filesystem, this UDTF could target
    // all agents.
    return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_PEM;
  }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                "The short ID of the agent", types::SemanticType::ST_ASID),
        ColInfo("address", types::DataType::STRING, types::PatternType::GENERAL, "address space"),
        ColInfo("offset", types::DataType::STRING, types::PatternType::GENERAL,
                "offset into the mapping"),
        ColInfo("pathname", types::DataType::STRING, types::PatternType::GENERAL,
                "name associated file"),
        ColInfo("size_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of the mapping", types::SemanticType::ST_BYTES),
        ColInfo("kernel_page_size_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "size of each page allocated when backing a VMA", types::SemanticType::ST_BYTES),
        ColInfo("mmu_page_size_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "page size used by the MMU when backing a VMA", types::SemanticType::ST_BYTES),
        ColInfo("rss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "resident set memory size", types::SemanticType::ST_BYTES),
        ColInfo("pss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "proportional set size", types::SemanticType::ST_BYTES),
        ColInfo("shared_clean_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "clean shared pages in the mapping", types::SemanticType::ST_BYTES),
        ColInfo("shared_dirty_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "dirty shared pages in the mapping", types::SemanticType::ST_BYTES),
        ColInfo("private_clean_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "clean private pages in the mapping", types::SemanticType::ST_BYTES),
        ColInfo("private_dirty_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "dirty private pages in the mapping", types::SemanticType::ST_BYTES),
        ColInfo("referenced_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "memory currently marked as referenced or accessed", types::SemanticType::ST_BYTES),
        ColInfo("anonymous_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "memory that does not belong to any file", types::SemanticType::ST_BYTES),
        ColInfo("lazy_free_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "memory which is marked by madvise", types::SemanticType::ST_BYTES),
        ColInfo("anon_huge_pages_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "memory backed by transparent hugepage", types::SemanticType::ST_BYTES),
        ColInfo("shmem_pmd_mapped_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "shared (shmem/tmpfs) memory backed by huge pages", types::SemanticType::ST_BYTES),
        ColInfo("file_pmd_mapped_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "file_pmd_mapped_bytes", types::SemanticType::ST_BYTES),
        ColInfo("shared_hugetlb_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "shared memory backed by hugetlbfs page", types::SemanticType::ST_BYTES),
        ColInfo("private_hugetlb_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "private memory backed by hugetlbfs page", types::SemanticType::ST_BYTES),
        ColInfo("swap_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "anonymous memory on swap", types::SemanticType::ST_BYTES),
        ColInfo("swap_pss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "proportional swap share of the mapping", types::SemanticType::ST_BYTES),
        ColInfo("locked_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "locked_bytes", types::SemanticType::ST_BYTES));
  }

  Status Init(FunctionContext* ctx) {
    const ProcParser proc_parser;
    return proc_parser.ParseProcPIDSMaps(ctx->metadata_state()->pid(), &stats_);
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    if (static_cast<size_t>(current_idx_) >= stats_.size()) {
      return false;
    }
    auto smap = stats_[current_idx_];

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("address")>(smap.ToAddress());
    rw->Append<IndexOf("offset")>(stats_[current_idx_].offset);
    rw->Append<IndexOf("pathname")>(stats_[current_idx_].pathname);
    rw->Append<IndexOf("size_bytes")>(stats_[current_idx_].size_bytes);
    rw->Append<IndexOf("kernel_page_size_bytes")>(stats_[current_idx_].kernel_page_size_bytes);
    rw->Append<IndexOf("mmu_page_size_bytes")>(stats_[current_idx_].mmu_page_size_bytes);
    rw->Append<IndexOf("rss_bytes")>(stats_[current_idx_].rss_bytes);
    rw->Append<IndexOf("pss_bytes")>(stats_[current_idx_].pss_bytes);
    rw->Append<IndexOf("shared_clean_bytes")>(stats_[current_idx_].shared_clean_bytes);
    rw->Append<IndexOf("shared_dirty_bytes")>(stats_[current_idx_].shared_dirty_bytes);
    rw->Append<IndexOf("private_clean_bytes")>(stats_[current_idx_].private_clean_bytes);
    rw->Append<IndexOf("private_dirty_bytes")>(stats_[current_idx_].private_dirty_bytes);
    rw->Append<IndexOf("referenced_bytes")>(stats_[current_idx_].referenced_bytes);
    rw->Append<IndexOf("anonymous_bytes")>(stats_[current_idx_].anonymous_bytes);
    rw->Append<IndexOf("lazy_free_bytes")>(stats_[current_idx_].lazy_free_bytes);
    rw->Append<IndexOf("anon_huge_pages_bytes")>(stats_[current_idx_].anon_huge_pages_bytes);
    rw->Append<IndexOf("shmem_pmd_mapped_bytes")>(stats_[current_idx_].shmem_pmd_mapped_bytes);
    rw->Append<IndexOf("file_pmd_mapped_bytes")>(stats_[current_idx_].file_pmd_mapped_bytes);
    rw->Append<IndexOf("shared_hugetlb_bytes")>(stats_[current_idx_].shared_hugetlb_bytes);
    rw->Append<IndexOf("private_hugetlb_bytes")>(stats_[current_idx_].private_hugetlb_bytes);
    rw->Append<IndexOf("swap_bytes")>(stats_[current_idx_].swap_bytes);
    rw->Append<IndexOf("swap_pss_bytes")>(stats_[current_idx_].swap_pss_bytes);
    rw->Append<IndexOf("locked_bytes")>(stats_[current_idx_].locked_bytes);

    ++current_idx_;
    return static_cast<size_t>(current_idx_) < stats_.size();
  }

 private:
  std::vector<ProcParser::ProcessSMaps> stats_;
  int current_idx_ = 0;
};

class HeapReleaseFreeMemoryUDTF final : public carnot::udf::UDTF<HeapReleaseFreeMemoryUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent", types::SemanticType::ST_ASID));
  }

  Status Init(FunctionContext*) { return Status::OK(); }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    px::ReleaseFreeMemory();
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    return false;
  }
};

class HeapRangesUDTF final : public carnot::udf::UDTF<HeapRangesUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }
  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                "The short ID of the agent.", types::SemanticType::ST_ASID),
        ColInfo("address", types::DataType::INT64, types::PatternType::GENERAL,
                "The address of the memory region.", types::SemanticType::ST_NONE),
        ColInfo("type", types::DataType::STRING, types::PatternType::GENERAL_ENUM,
                "The type of memory range (INUSE, FREE, UNMAPPED, or UNKNOWN).",
                types::SemanticType::ST_NONE),
        ColInfo("length", types::DataType::INT64, types::PatternType::GENERAL,
                "The size of the range in bytes.", types::SemanticType::ST_BYTES),
        ColInfo("inuse_fraction", types::DataType::FLOAT64, types::PatternType::GENERAL,
                "The fraction of the range that is in use, if the type is not INUSE then this "
                "value is 0.",
                types::SemanticType::ST_NONE));
  }

  Status Init(FunctionContext*) {
#ifdef TCMALLOC
    auto range_func = [](void* udtf, const ::base::MallocRange* range) {
      static_cast<HeapRangesUDTF*>(udtf)->ranges_.push_back(*range);
    };
    MallocExtension::instance()->Ranges(this, range_func);
#endif
    return Status::OK();
  }
  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    if (idx_ >= ranges_.size()) {
      return false;
    }
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("address")>(static_cast<int64_t>(ranges_[idx_].address));
    rw->Append<IndexOf("type")>(std::string(magic_enum::enum_name(ranges_[idx_].type)));
    rw->Append<IndexOf("length")>(ranges_[idx_].length);
    rw->Append<IndexOf("inuse_fraction")>(ranges_[idx_].fraction);
    idx_++;
    return idx_ < ranges_.size();
#else
    PX_UNUSED(ctx);
    PX_UNUSED(rw);
    return false;
#endif
  }

 private:
#ifdef TCMALLOC
  size_t idx_ = 0;
  std::vector<::base::MallocRange> ranges_;
#endif
};

class HeapStatsNumericUDTF final : public carnot::udf::UDTF<HeapStatsNumericUDTF> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                "The short ID of the agent"),
        ColInfo("current_allocated_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "Number of bytes currently allocated by agent", types::SemanticType::ST_BYTES),
        ColInfo("heap_size_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "Size of the agent's heap in bytes. Includes allocated, free, and unmapped bytes",
                types::SemanticType::ST_BYTES),
        ColInfo("central_cache_free_bytes", types::DataType::INT64,
                types::PatternType::METRIC_GAUGE,
                "Number of free bytes in tcmalloc's central cache", types::SemanticType::ST_BYTES),
        ColInfo("transfer_cache_free_bytes", types::DataType::INT64,
                types::PatternType::METRIC_GAUGE,
                "Number of free bytes in tcmalloc's transfer cache", types::SemanticType::ST_BYTES),
        ColInfo("thread_cache_free_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "Number of free bytes in tcmalloc's thread cache", types::SemanticType::ST_BYTES),
        ColInfo("pageheap_free_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "Number of free bytes in tcmalloc's pageheap", types::SemanticType::ST_BYTES),
        ColInfo("pageheap_unmapped_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
                "Number of unmapped bytes in tcmalloc's pageheap", types::SemanticType::ST_BYTES));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
#ifdef TCMALLOC
    size_t current_allocated_bytes, heap_size, central_cache_free, transfer_cache_free,
        thread_cache_free, pageheap_free, pageheap_unmapped;
    MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes",
                                                    &current_allocated_bytes);
    MallocExtension::instance()->GetNumericProperty("generic.heap_size", &heap_size);
    MallocExtension::instance()->GetNumericProperty("tcmalloc.central_cache_free_bytes",
                                                    &central_cache_free);
    MallocExtension::instance()->GetNumericProperty("tcmalloc.transfer_cache_free_bytes",
                                                    &transfer_cache_free);
    MallocExtension::instance()->GetNumericProperty("tcmalloc.thread_cache_free_bytes",
                                                    &thread_cache_free);
    MallocExtension::instance()->GetNumericProperty("tcmalloc.pageheap_free_bytes", &pageheap_free);
    MallocExtension::instance()->GetNumericProperty("tcmalloc.pageheap_unmapped_bytes",
                                                    &pageheap_unmapped);

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("current_allocated_bytes")>(current_allocated_bytes);
    rw->Append<IndexOf("heap_size_bytes")>(heap_size);
    rw->Append<IndexOf("central_cache_free_bytes")>(central_cache_free);
    rw->Append<IndexOf("transfer_cache_free_bytes")>(transfer_cache_free);
    rw->Append<IndexOf("thread_cache_free_bytes")>(thread_cache_free);
    rw->Append<IndexOf("pageheap_free_bytes")>(pageheap_free);
    rw->Append<IndexOf("pageheap_unmapped_bytes")>(pageheap_unmapped);
#else
    PX_UNUSED(ctx);
    PX_UNUSED(rw);
#endif
    return false;
  }
};

}  // namespace funcs
}  // namespace vizier
}  // namespace px
