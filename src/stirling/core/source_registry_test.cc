#include "src/stirling/core/source_registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"

namespace pl {
namespace stirling {

void RegisterTestSources(SourceRegistry* registry) {
  registry->RegisterOrDie<SeqGenConnector>("source_0");
  registry->RegisterOrDie<SeqGenConnector>("source_1");
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
    auto iter = registry_.sources().find("source_0");
    auto element = iter->second;
    ASSERT_NE(registry_.sources().end(), iter);
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->source_name());
  }

  {
    name = "proc_stat";
    auto iter = registry_.sources().find("source_1");
    ASSERT_NE(registry_.sources().end(), iter);
    auto element = iter->second;
    auto source_fn = element.create_source_fn;
    auto source = source_fn(name);
    EXPECT_EQ(name, source->source_name());
  }

  auto all_sources = registry_.sources();
  EXPECT_EQ(2, all_sources.size());
}

}  // namespace stirling
}  // namespace pl
