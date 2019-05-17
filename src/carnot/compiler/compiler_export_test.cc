#include "src/carnot/compiler/compiler_export.h"

#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/error.h"
#include "src/common/base/macros.h"
#include "src/common/base/statusor.h"
namespace pl {
namespace carnot {
namespace compiler {
StatusOr<std::string> CompilerCompileGoStr(CompilerPtr compiler_ptr, std::string schema,
                                           std::string tableName, std::string query,
                                           int* resultLen) {
  char* result = CompilerCompile(compiler_ptr, schema.c_str(), schema.length(), tableName.c_str(),
                                 tableName.length(), query.c_str(), query.length(), resultLen);
  if (*resultLen == 0) {
    return error::InvalidArgument("Compiler failed to return.");
  }

  std::string lp_str(result, result + *resultLen);
  delete[] result;
  return lp_str;
}

class CompilerExportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    compiler_ = CompilerNew();
    // Setup the schema from a proto.
    rel_proto_ = R"(columns {
      column_name: "_time"
      column_type: TIME64NS
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
    }
    columns {
      column_name: "tlb_misses"
      column_type: INT64
    }
    columns {
      column_name: "http"
      column_type: INT64
    })";
    table_name_ = "perf_and_http";
  }
  void TearDown() override { CompilerFree(compiler_); }
  CompilerPtr compiler_;
  std::string rel_proto_;
  std::string table_name_;
};

TEST_F(CompilerExportTest, query_test) {
  // Pass the relation proto, table and query to the compilation.
  const std::string expected_plan = R"proto(
  status {
  }
  logical_plan {
    dag {
      nodes {
        id: 1
      }
    }
    nodes {
      id: 1
      dag {
        nodes {
          sorted_deps: 7
        }
        nodes {
          id: 7
          sorted_deps: 13
        }
        nodes {
          id: 13
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "perf_and_http"
            column_idxs: 0
            column_idxs: 1
            column_idxs: 2
            column_idxs: 3
            column_names: "_time"
            column_names: "cpu_cycles"
            column_names: "tlb_misses"
            column_names: "http"
            column_types: TIME64NS
            column_types: INT64
            column_types: INT64
            column_types: INT64
          }
        }
      }
      nodes {
        id: 7
        op {
          op_type: MAP_OPERATOR
          map_op {
            expressions {
              column {
                index: 3
              }
            }
            expressions {
              func {
                name: "pl.divide"
                args {
                  column {
                    index: 1
                  }
                }
                args {
                  column {
                    index: 2
                  }
                }
                args_data_types: INT64
                args_data_types: INT64
              }
            }
            column_names: "http_code"
            column_names: "cpu_tlb_ratio"
          }
        }
      }
      nodes {
        id: 13
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "out"
            column_types: INT64
            column_types: INT64
            column_names: "http_code"
            column_names: "cpu_tlb_ratio"
          }
        }
      }
    }
  }
  )proto";

  int result_len;
  std::vector<std::string> query_lines{
      "queryDF = From(table='perf_and_http', select=['_time', 'cpu_cycles', 'tlb_misses', 'http'])",
      "mapDF = queryDF.Map(fn=lambda r : {'http_code' : r.http, 'cpu_tlb_ratio' : "
      "r.cpu_cycles/r.tlb_misses})",
      "mapDF.Result(name='out')",
  };
  auto compiler_interface_result = CompilerCompileGoStr(
      compiler_, rel_proto_, table_name_, absl::StrJoin(query_lines, "\n"), &result_len);
  ASSERT_OK(compiler_interface_result);

  compilerpb::CompilerResult compiler_result_pb;
  ASSERT_TRUE(compiler_result_pb.ParseFromString(compiler_interface_result.ConsumeValueOrDie()));
  EXPECT_OK(compiler_result_pb.status());

  compilerpb::CompilerResult expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(expected_plan, &expected_pb));

  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, compiler_result_pb));
}

TEST_F(CompilerExportTest, bad_queries) {
  int result_len;
  // Bad table name query.
  std::vector<std::string> bad_table_query{
      "queryDF = From(table='bad_table_name', select=['_time', 'cpu_cycles', 'tlb_misses', "
      "'http'])",
      "mapDF = queryDF.Map(fn=lambda r : {'http_code' : r.http, 'cpu_tlb_ratio' : "
      "r.cpu_cycles/r.tlb_misses})",
      "mapDF.Result(name='out')",
  };
  auto compiler_interface_result = CompilerCompileGoStr(
      compiler_, rel_proto_, table_name_, absl::StrJoin(bad_table_query, "\n"), &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(compiler_interface_result);
  compilerpb::CompilerResult compiler_result_pb;
  ASSERT_TRUE(compiler_result_pb.ParseFromString(compiler_interface_result.ConsumeValueOrDie()));
  EXPECT_NOT_OK(compiler_result_pb.status());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
