#include <gtest/gtest.h>

#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/bcc_wrapper.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/source_registry.h"

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
    auto s = registry_.GetRegistryElement("test_fake_proc_cpu_source");
    EXPECT_OK(s);
    auto& element = s.ValueOrDie();
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->source_name());
  }

  {
    name = "proc_stat";
    auto s = registry_.GetRegistryElement("test_proc_stat_source");
    EXPECT_OK(s);
    auto& element = s.ValueOrDie();
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->source_name());
  }

  // Unavailable source connectors should not make their way into the registry.
  auto s = registry_.GetRegistryElement("unavailable_source");
  EXPECT_FALSE(s.ok());

  auto all_sources = registry_.sources();
  EXPECT_EQ(2, all_sources.size());
}

}  // namespace stirling
}  // namespace pl
