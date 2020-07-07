#include "src/stirling/dynamic_tracing/goid.h"

#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_tracing/code_gen.h"

DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::proto::EqualsProto;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(GOIDTest, CheckProbe) {
  ir::logical::Program program;
  GenGOIDEntryProbe(&program);
  PL_LOG_VAR(program.DebugString());
  EXPECT_THAT(program, Partially(EqualsProto(
                           R"proto(
                                       maps {
                                         name: "pid_goid_map"
                                       }
                                       probes {
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
                                           key_expr: "tgid_pid()"
                                           value_variable_name: "goid_"
                                         }
                                       }
                                       )proto")));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
