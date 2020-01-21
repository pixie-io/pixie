#include "src/stirling/source_registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/proc_stat_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(DummyUnavailableConnector);

void RegisterTestSources(SourceRegistry* registry) {
  registry->RegisterOrDie<FakeProcStatConnector>("test_fake_proc_cpu_source");
  registry->RegisterOrDie<ProcStatConnector>("test_proc_stat_source");
  registry->RegisterOrDie<DummyUnavailableConnector>("unavailable_source");
}

class SourceRegistryTest : public ::testing::Test {
 protected:
  SourceRegistryTest() = default;
  void SetUp() override { RegisterTestSources(&registry_); }
  SourceRegistry registry_;
};

TEST_F(SourceRegistryTest, register_sources) {
  std::string name;
  {
    name = "fake_proc_stat";
    ASSERT_OK_AND_ASSIGN(auto element, registry_.GetRegistryElement("test_fake_proc_cpu_source"));
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->source_name());
  }

  {
    name = "proc_stat";
    ASSERT_OK_AND_ASSIGN(auto element, registry_.GetRegistryElement("test_proc_stat_source"));
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->source_name());
  }

  // Unavailable source connectors should not make their way into the registry.
  EXPECT_NOT_OK(registry_.GetRegistryElement("unavailable_source"));

  auto all_sources = registry_.sources();
  EXPECT_EQ(2, all_sources.size());
}

}  // namespace stirling
}  // namespace pl
