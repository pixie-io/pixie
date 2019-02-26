#include <gtest/gtest.h>
#include <thread>

#include "src/common/utils.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

using stirlingpb::Element_State;
using types::DataType;

// This test does not currently run because it requires:
//  1) Linux
//  2) root access
//  3) access to BPF
// Most systems (including the build environment) do not support all three.
//
// In fact, this code has never been tested! Do not trust!
// TODO(oazizi): Pick this up at a future point in time, if deemed valuable.

TEST(BPFTraceConnectorTest, bpftrace_connector_all) {
  std::unique_ptr<SourceConnector> bpftrace_connector =
      CPUStatBPFTraceConnector::Create(CPUStatBPFTraceConnector::kName);

  Status s;

  ASSERT_TRUE(IsRoot());

  EXPECT_OK(bpftrace_connector->Init());

  auto elements = bpftrace_connector->elements();
  uint32_t num_fields = bpftrace_connector->elements().size();
  uint32_t field_size_bytes = 8;

  for (uint32_t ifield = 0; ifield < num_fields; ++ifield) {
    ASSERT_EQ(field_size_bytes, elements[ifield].WidthBytes());
  }

  for (uint32_t i = 0; i < 5; ++i) {
    RawDataBuf data = bpftrace_connector->GetData();
    auto int64_buf = reinterpret_cast<int64_t*>(data.buf);
    auto double_buf = reinterpret_cast<double*>(data.buf);

    // Currently counting zeros
    // TODO(oazizi): Add something more interesting/robust if we revive this test.
    uint32_t count_zeros = 0;

    for (uint32_t irecord = 0; irecord < data.num_records; ++irecord) {
      for (uint32_t ifield = 0; ifield < num_fields; ++ifield) {
        uint32_t buf_idx = irecord * field_size_bytes + ifield;

        DataType type = elements[ifield].type();
        switch (type) {
          case DataType::INT64: {
            auto val = int64_buf[buf_idx];
            if (val == 0) {
              count_zeros++;
            }
          } break;
          case DataType::FLOAT64: {
            auto val = double_buf[buf_idx];
            if (val == 0) {
              count_zeros++;
            }
          } break;
          default:
            ASSERT_TRUE(false);
        }

        EXPECT_NE(0ULL, count_zeros);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  EXPECT_OK(bpftrace_connector->Stop());
}

}  // namespace stirling
}  // namespace pl
