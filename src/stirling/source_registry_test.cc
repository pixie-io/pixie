#include <gtest/gtest.h>

#include "src/common/types/types.pb.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

std::unique_ptr<SourceConnector> CreateFakeTestEBPFSource() {
  std::vector<InfoClassElement> elements = {};
  std::unique_ptr<SourceConnector> source_ptr =
      std::make_unique<EBPFConnector>("fake_ebpf", elements, "", "", "");
  return source_ptr;
}

std::unique_ptr<SourceConnector> CreateFakeTestProcStatSource() {
  std::vector<InfoClassElement> elements = {};
  std::unique_ptr<SourceConnector> source_ptr =
      std::make_unique<ProcStatConnector>("fake_proc_stat", elements);
  return source_ptr;
}

void RegisterFakeTestSources(SourceRegistry* registry) {
  SourceRegistry::RegistryElement ebpf_cpu_source_element(SourceType::kEBPF,
                                                          CreateFakeTestEBPFSource);
  registry->RegisterOrDie("fake_ebpf_cpu_source", ebpf_cpu_source_element);

  SourceRegistry::RegistryElement proc_stat_source_element(SourceType::kFile,
                                                           CreateFakeTestProcStatSource);
  registry->RegisterOrDie("fake_proc_stat_source", proc_stat_source_element);
}

class SourceRegistryTest : public ::testing::Test {
 protected:
  SourceRegistryTest() : registry_("metrics") {}
  void SetUp() override { RegisterFakeTestSources(&registry_); }
  SourceRegistry registry_;
};

TEST_F(SourceRegistryTest, validate_registry) { EXPECT_EQ("metrics", registry_.name()); }

TEST_F(SourceRegistryTest, register_sources) {
  auto s = registry_.GetRegistryElement("fake_ebpf_cpu_source");
  EXPECT_OK(s);
  auto element = s.ValueOrDie();
  EXPECT_EQ(SourceType::kEBPF, element.type);
  auto source_fn = element.create_source_fn;
  auto source = source_fn();
  EXPECT_EQ("fake_ebpf", source->source_name());
  EXPECT_EQ(SourceType::kEBPF, source->type());

  s = registry_.GetRegistryElement("fake_proc_stat_source");
  EXPECT_OK(s);
  element = s.ValueOrDie();
  EXPECT_EQ(SourceType::kFile, element.type);
  source_fn = element.create_source_fn;
  source = source_fn();
  EXPECT_EQ("fake_proc_stat", source->source_name());
  EXPECT_EQ(SourceType::kFile, source->type());

  auto all_sources = registry_.sources_map();
  EXPECT_EQ(2, all_sources.size());
}

}  // namespace stirling
}  // namespace pl
