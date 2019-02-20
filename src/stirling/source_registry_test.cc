#include <gtest/gtest.h>

#include "src/common/types/types.pb.h"
#include "src/stirling/bcc_connector.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

void RegisterTestSources(SourceRegistry* registry) {
  registry->RegisterOrDie<BCCCPUMetricsConnector>("test_ebpf_cpu_source");
  registry->RegisterOrDie<ProcStatConnector>("test_proc_stat_source");
}

class SourceRegistryTest : public ::testing::Test {
 protected:
  SourceRegistryTest() : registry_("metrics") {}
  void SetUp() override { RegisterTestSources(&registry_); }
  SourceRegistry registry_;
};

TEST_F(SourceRegistryTest, validate_registry) { EXPECT_EQ("metrics", registry_.name()); }

TEST_F(SourceRegistryTest, register_sources) {
  auto s = registry_.GetRegistryElement("test_ebpf_cpu_source");
  EXPECT_OK(s);
  auto element = s.ValueOrDie();
  EXPECT_EQ(SourceType::kEBPF, element.type);
  auto source_fn = element.create_source_fn;
  auto source = source_fn();
  EXPECT_EQ("ebpf_cpu_metrics", source->source_name());
  EXPECT_EQ(SourceType::kEBPF, source->type());

  s = registry_.GetRegistryElement("test_proc_stat_source");
  EXPECT_OK(s);
  element = s.ValueOrDie();
  EXPECT_EQ(SourceType::kFile, element.type);
  source_fn = element.create_source_fn;
  source = source_fn();
  EXPECT_EQ("proc_stat", source->source_name());
  EXPECT_EQ(SourceType::kFile, source->type());

  auto all_sources = registry_.sources_map();
  EXPECT_EQ(2, all_sources.size());
}

}  // namespace stirling
}  // namespace pl
