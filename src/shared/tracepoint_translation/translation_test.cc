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

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "src/common/testing/event/simulated_time_system.h"
#include "src/common/testing/protobuf.h"

#include "src/common/testing/testing.h"
#include "src/shared/tracepoint_translation/translation.h"

namespace px {
namespace tracepoint {

using ::google::protobuf::TextFormat;
using ::testing::_;

constexpr char kPlannerTracepoint[] = R"(
name: "grpc_probe"
ttl {
  seconds: 3600
  nanos: 1234
}
programs {
  table_name: "grpc_probe_table"
  spec {
    outputs {
      name: "probe_WriteDataPadded_table"
      fields: "stream_id"
      fields: "end_stream"
      fields: "latency"
    }
    probe {
      name: "probe_WriteDataPadded"
      tracepoint: {
        symbol: "golang.org/x/net/http2.(*Framer).WriteDataPadded"
      }
      args {
        id: "stream_id"
        expr: "streamID"
      }
      args {
        id: "end_stream"
        expr: "endStream"
      }
      ret_vals {
        id: "error"
        expr: "error"
      }
      function_latency {
        id: "latency"
      }
      output_actions {
        output_name: "probe_WriteDataPadded_table"
        variable_names: "stream_id"
        variable_names: "end_stream"
        variable_names: "latency"
      }
    }
  }
}
)";

constexpr char kStirlingTracepoint[] = R"(
name: "grpc_probe"
ttl {
  seconds: 3600
  nanos: 1234
}
tracepoints {
  table_name: "grpc_probe_table"
  program {
    outputs {
      name: "probe_WriteDataPadded_table"
      fields: "stream_id"
      fields: "end_stream"
      fields: "latency"
    }
    probes {
      name: "probe_WriteDataPadded"
      tracepoint {
        symbol: "golang.org/x/net/http2.(*Framer).WriteDataPadded"
        type: LOGICAL
      }
      args {
        id: "stream_id"
        expr: "streamID"
      }
      args {
        id: "end_stream"
        expr: "endStream"
      }
      ret_vals {
        id: "error"
        expr: "error"
      }
      function_latency {
        id: "latency"
      }
      output_actions {
        output_name: "probe_WriteDataPadded_table"
        variable_names: "stream_id"
        variable_names: "end_stream"
        variable_names: "latency"
      }
    }
  }
}
)";

class TracepointManagerTest : public ::testing::Test {};

using PlannerDeployment = ::px::carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment;
using StirlingDeployment = ::px::stirling::dynamic_tracing::ir::logical::TracepointDeployment;
TEST_F(TracepointManagerTest, TracepointConversion) {
  PlannerDeployment planner_deployment;
  CHECK(TextFormat::ParseFromString(kPlannerTracepoint, &planner_deployment));

  StirlingDeployment stirling_deployment;

  ConvertPlannerTracepointToStirlingTracepoint(planner_deployment, &stirling_deployment);

  EXPECT_THAT(stirling_deployment,
              testing::proto::Partially(testing::proto::EqualsProto(kStirlingTracepoint)));
}

constexpr char kPlannerUPIDDeployment[] = R"(
deployment_spec {
  upid {
    asid: 123
    pid: 4567
    ts_ns: 1234567890
  }
}
)";

constexpr char kStirlingUPIDDeployment[] = R"(
deployment_spec {
  upid_list {
    upids {
      asid: 123
      pid: 4567
      ts_ns: 1234567890
    }
  }
}
)";

constexpr char kPlannerPodProcessDeployment[] = R"(
deployment_spec {
  pod_process {
    pods: "abcd"
    container: "efgh"
    process: "12345"
  }
}
)";

constexpr char kStirlingPodProcessDeployment[] = R"(
deployment_spec {
  pod_process {
    pods: "abcd"
    container: "efgh"
    process: "12345"
  }
}
)";

constexpr char kPlannerSharedObjectDeployment[] = R"(
deployment_spec {
  shared_object {
    name: "shared_abcd"
    upid {
      asid: 123
      pid: 4567
      ts_ns: 1234567890
    }
  }
}
)";

constexpr char kStirlingSharedObjectDeployment[] = R"(
deployment_spec {
  shared_object {
    name: "shared_abcd"
    upid {
      asid: 123
      pid: 4567
      ts_ns: 1234567890
    }
  }
}
)";

TEST_F(TracepointManagerTest, UPIDDeployment) {
  PlannerDeployment planner_deployment;
  CHECK(TextFormat::ParseFromString(kPlannerUPIDDeployment, &planner_deployment));

  StirlingDeployment stirling_deployment;

  ConvertPlannerTracepointToStirlingTracepoint(planner_deployment, &stirling_deployment);

  EXPECT_THAT(stirling_deployment,
              testing::proto::Partially(testing::proto::EqualsProto(kStirlingUPIDDeployment)));
}

TEST_F(TracepointManagerTest, PodProcessDeployment) {
  PlannerDeployment planner_deployment;
  CHECK(TextFormat::ParseFromString(kPlannerPodProcessDeployment, &planner_deployment));

  StirlingDeployment stirling_deployment;

  ConvertPlannerTracepointToStirlingTracepoint(planner_deployment, &stirling_deployment);

  EXPECT_THAT(stirling_deployment, testing::proto::Partially(
                                       testing::proto::EqualsProto(kStirlingPodProcessDeployment)));
}

TEST_F(TracepointManagerTest, SharedObjectDeployment) {
  PlannerDeployment planner_deployment;
  CHECK(TextFormat::ParseFromString(kPlannerSharedObjectDeployment, &planner_deployment));

  StirlingDeployment stirling_deployment;

  ConvertPlannerTracepointToStirlingTracepoint(planner_deployment, &stirling_deployment);

  EXPECT_THAT(
      stirling_deployment,
      testing::proto::Partially(testing::proto::EqualsProto(kStirlingSharedObjectDeployment)));
}

constexpr char kPlannerBPFTraceDeployment[] = R"(
programs {
  bpftrace {
    program: "create a table a and more bpftrace code"
  }
}
)";

constexpr char kStirlingBPFTraceDeployment[] = R"(
tracepoints {
  bpftrace {
    program: "create a table a and more bpftrace code"
  }
}
)";

TEST_F(TracepointManagerTest, BPFTraceDeployment) {
  PlannerDeployment planner_deployment;
  CHECK(TextFormat::ParseFromString(kPlannerBPFTraceDeployment, &planner_deployment));

  StirlingDeployment stirling_deployment;

  ConvertPlannerTracepointToStirlingTracepoint(planner_deployment, &stirling_deployment);

  EXPECT_THAT(stirling_deployment,
              testing::proto::Partially(testing::proto::EqualsProto(kStirlingBPFTraceDeployment)));
}

}  // namespace tracepoint
}  // namespace px
