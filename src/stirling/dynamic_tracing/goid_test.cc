#include "src/stirling/dynamic_tracing/goid.h"

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/code_gen.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::proto::EqualsProto;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(GOIDTest, CheckMap) {
  EXPECT_THAT(GenGOIDMap(), EqualsProto(R"proto(name: "pid_goid_map")proto"));
}

TEST(GOIDTest, CheckProbe) {
  EXPECT_THAT(GenGOIDProbe(), Partially(EqualsProto(
                                  R"proto(
                                        name: "probe_entry_runtime_casgstatus"
                                        trace_point {
                                          symbol: "runtime.casgstatus"
                                          type: ENTRY
                                        }
                                        consts {
                                          name: "kGRunningState"
                                          type: INT64
                                          constant: "2"
                                        }
                                        args {
                                          id: "goid_"
                                          expr: "gp.goid"
                                        }
                                        args {
                                          id: "newval"
                                          expr: "newval"
                                        }
                                        map_stash_actions {
                                          map_name: "pid_goid_map"
                                          key: TGID_PID
                                          value_variable_name: "goid_"
                                        }
                                        )proto")));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
