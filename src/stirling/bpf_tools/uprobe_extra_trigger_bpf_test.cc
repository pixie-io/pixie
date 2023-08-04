/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/system.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_19_grpc_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_1_19_grpc_server_container.h"

namespace px {
namespace stirling {
namespace bpf_tools {

constexpr char kBCCProgram[] = R"(
  BPF_ARRAY(count, int, 1);

  int probe_write_data_padded(struct pt_regs* ctx) {
    int kIndex = 0;

    int* count_ptr = count.lookup(&kIndex);
    if (count_ptr == NULL) {
      return 0;
    }

    *count_ptr += 1;

    return 0;
  }
)";

// This test shows an unexpected extra uprobe trigger that can happen due
// to two seemingly different paths pointing to the same file.
// It is not enough to resolve symlinks, because the two paths may come from two
// instances of the same container image, which appear to be separate paths,
// but which have share the same file inode behind the scenes.
// Disabling this test since this bug doesn't seem to reproduce with podman.
TEST(BCCWrapper, DISABLED_UnexpectedExtraTrigger) {
  BCCWrapper bcc_wrapper;
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kBCCProgram));

  ::px::stirling::testing::Go1_19_GRPCServerContainer server1;
  ::px::stirling::testing::Go1_19_GRPCServerContainer server2;
  ::px::stirling::testing::Go1_19_GRPCClientContainer client1;
  ::px::stirling::testing::Go1_19_GRPCClientContainer client2;

  // A Uprobe template for the GRPCServerContainer.
  // Binary path is set later.
  UProbeSpec probe_spec = {
      .binary_path = "",
      .symbol = "net/http.(*http2Framer).WriteDataPadded",
      .attach_type = bpf_tools::BPFProbeAttachType::kEntry,
      .probe_fn = "probe_write_data_padded",
  };

  // A templated path to the server. We will replace $0 with the pid of the server instance.
  const std::string kServerPath = "/proc/$0/root/golang_1_19_grpc_tls_server_binary";

  // Run server 1 and attach uprobes to it.
  ASSERT_OK(server1.Run(std::chrono::seconds{60}));
  probe_spec.binary_path = absl::Substitute(kServerPath, std::to_string(server1.process_pid()));
  ASSERT_OK(bcc_wrapper.AttachUProbe(probe_spec));

  // Use a client to send a single request to server.
  ASSERT_OK(client1.Run(std::chrono::seconds{10},
                        {absl::Substitute("--network=container:$0", server1.container_name())}));
  client1.Wait();

  // We expect the probe to be triggered once, and it is.
  auto counts = WrappedBCCArrayTable<int>::Create(&bcc_wrapper, "count");
  ASSERT_OK_AND_ASSIGN(const int trigger_count1, counts->GetValue(0));
  ASSERT_EQ(trigger_count1, 1);

  // Now spawn a second server in a separate container, and attach a uprobe to it.
  // Note this server will not be stimulated in any way.
  ASSERT_OK(server2.Run(std::chrono::seconds{60}));
  probe_spec.binary_path = absl::Substitute(kServerPath, std::to_string(server2.process_pid()));
  ASSERT_OK(bcc_wrapper.AttachUProbe(probe_spec));

  // Use a client to send a single request to original server1, not server2.
  ASSERT_OK(client2.Run(std::chrono::seconds{10},
                        {absl::Substitute("--network=container:$0", server1.container_name())}));
  client2.Wait();

  // We expect the probe to be triggered one additional time, but it's actually triggered twice.
  ASSERT_OK_AND_ASSIGN(const int trigger_count2, counts->GetValue(0));
  ASSERT_EQ(trigger_count2, 3);

  // Why does this happen?
  // While the two containers have different file paths, they actually point to the same
  // file under the hood (they have the same inode number). Thus, while we we're attaching two
  // uprobes on what we think are separate binaries, we are actually putting two uprobes on the
  // same function of a single binary.
  //
  // So when the client sends a request to server1, both uprobes trigger,
  // resulting in unexpected behavior.
  //
  // TODO(oazizi): Make detection of the binary already being instrumented based on inode
  //               instead of path.
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
