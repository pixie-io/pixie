#include <gtest/gtest.h>

#include "src/common/types/types.pb.h"
#include "src/data_collector/ebpf_sources.h"
#include "src/data_collector/source_registry.h"

namespace pl {
namespace datacollector {

class SourceRegistryTest : public ::testing::Test {
 protected:
  SourceRegistryTest() : registry_("ebpf_metrics") {}
  void SetUp() override {
    std::unique_ptr<SourceConnector> source_ptr = std::make_unique<EBPFConnector>(
        "test_ebpf_source", elements_, "k_event", "fn_name", "bpf_program");
    registry_.RegisterOrDie("test_ebpf_source", std::move(source_ptr));
  }

  std::vector<InfoClassElement> elements_{
      InfoClassElement("_time", DataType::INT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("cpu_percentage", DataType::FLOAT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED)};
  SourceRegistry registry_;
};

TEST_F(SourceRegistryTest, validate_registry) { EXPECT_EQ("ebpf_metrics", registry_.name()); }

TEST_F(SourceRegistryTest, register_sources) {
  auto s = registry_.GetSource("test_ebpf_source");
  EXPECT_OK(s);
  auto source = s.ValueOrDie();
  EXPECT_EQ("test_ebpf_source", source->source_name());
  auto elements = source->elements();
  EXPECT_EQ(pl::types::DataType::INT64, elements[0].type());
  EXPECT_EQ("_time", elements[0].name());
  EXPECT_EQ(2, elements.size());
  EXPECT_EQ(pl::types::DataType::FLOAT64, elements[1].type());
}

}  // namespace datacollector
}  // namespace pl
