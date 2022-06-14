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

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/goid.h"

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/code_gen.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::px::testing::BazelRunfilePath;
using ::px::testing::proto::EqualsProto;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(GOIDTest, CheckMap) {
  EXPECT_THAT(GenGOIDMap(), EqualsProto(R"proto(name: "pid_goid_map")proto"));
}

TEST(GOIDTest, CheckProbe) {
  EXPECT_THAT(GenGOIDProbe(), Partially(EqualsProto(
                                  R"proto(
                                        name: "probe_entry_runtime_casgstatus"
                                        tracepoint {
                                          symbol: "runtime.casgstatus"
                                          type: ENTRY
                                        }
                                        consts {
                                          name: "kGRunningState"
                                          type: INT64
                                          constant: "2"
                                        }
                                        args {
                                          id: "goid"
                                          expr: "gp.goid"
                                        }
                                        args {
                                          id: "newval"
                                          expr: "newval"
                                        }
                                        map_stash_actions {
                                          map_name: "pid_goid_map"
                                          key: TGID_PID
                                          value_variable_names: "goid"
                                        }
                                        )proto")));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
