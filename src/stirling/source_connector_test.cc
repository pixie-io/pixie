#include <gtest/gtest.h>

#include "src/common/types/types.pb.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

using stirlingpb::Element_State;
using types::DataType;

TEST(SourceConnectorTest, create_ebpf_source) {
  std::vector<InfoClassElement> source_elements = {
      InfoClassElement("_time", DataType::INT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("cpu_id", DataType::INT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED),
      InfoClassElement("cpu_percentage", DataType::FLOAT64,
                       Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED)};
  EBPFConnector ebpf_source("ebpf_cpu_usage", source_elements, "kernel_event", "fn_name",
                            "bpf_program");
  SourceConnector* source_ptr = &ebpf_source;
  EXPECT_EQ("ebpf_cpu_usage", source_ptr->source_name());
  EXPECT_EQ(DataType::FLOAT64, source_ptr->elements()[2].type());
}

}  // namespace stirling
}  // namespace pl
