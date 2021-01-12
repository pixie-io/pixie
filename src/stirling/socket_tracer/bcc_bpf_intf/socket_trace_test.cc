#include <gtest/gtest.h>

// Needed for integer types used in socket_trace.h.
// We cannot include stdint.h inside socket_trace.h, as that conflicts with BCC's headers.
#include <cstdint>

#include "src/stirling/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

// Inside BPF, socket_data_event_t object is submitted as a bytes array, with a size calculated as
// sizeof(socket_data_event_t::attr_t) + <submitted msg size>
// We have to make sure that the follow condition holds, otherwise, the userspace code cannot
// accurately read data at the right boundary.
TEST(SocketDataEventTTest, VerifyAlignment) {
  socket_data_event_t event;
  EXPECT_EQ(0, offsetof(socket_data_event_t, attr));
  EXPECT_EQ(sizeof(event.attr), offsetof(socket_data_event_t, msg));
}
