#include <gtest/gtest.h>

#include "src/common/types/types.pb.h"
#include "src/stirling/bcc_connector.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/test_connector.h"

namespace pl {
namespace stirling {

using stirlingpb::Element_State;
using types::DataType;

TEST(SourceConnectorTest, create_ebpf_source) {
  auto ebpf_source = TestSourceConnector::Create();
  EXPECT_EQ("dummy_connector", ebpf_source->source_name());
  EXPECT_EQ(DataType::FLOAT64, ebpf_source->elements()[2].type());
}

}  // namespace stirling
}  // namespace pl
