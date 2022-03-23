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

#include "src/carnot/exec/otel_export_sink_node.h"

#include <utility>
#include <vector>

#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service_mock.grpc.pb.h"

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/carnotpb/carnot_mock.grpc.pb.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/shared/types/types.h"
#include "src/table_store/schemapb/schema.pb.h"

namespace px {
namespace carnot {
namespace exec {

using carnotpb::MockResultSinkServiceStub;
using carnotpb::ResultSinkService;
using carnotpb::TransferResultChunkRequest;
using carnotpb::TransferResultChunkResponse;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using testing::proto::EqualsProto;
namespace otelmetricscollector = opentelemetry::proto::collector::metrics::v1;

class OTelExportSinkNodeTest : public ::testing::Test {
 public:
  OTelExportSinkNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();

    mock_unique_ = std::make_unique<otelmetricscollector::MockMetricsServiceStub>();
    mock_ = mock_unique_.get();

    exec_state_ = std::make_unique<ExecState>(
        func_registry_.get(), table_store, MockResultSinkStubGenerator,
        [this](const std::string& url)
            -> std::unique_ptr<otelmetricscollector::MetricsService::StubInterface> {
          url_ = url;
          return std::move(mock_unique_);
        },
        sole::uuid4(), nullptr, nullptr, [](grpc::ClientContext*) {});

    table_store::schema::Relation rel({types::DataType::BOOLEAN, types::DataType::TIME64NS},
                                      {"col1", "time_"});
  }

 protected:
  std::string url_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  otelmetricscollector::MockMetricsServiceStub* mock_;

 private:
  // Ownership will be transferred to the GRPC node, so access this ptr via `mock_` in the tests.
  std::unique_ptr<otelmetricscollector::MockMetricsServiceStub> mock_unique_;
};

TEST_F(OTelExportSinkNodeTest, endpoint_config) {
  std::string operator_pb_txt = R"(
endpoint_config {
  url: "otlp.px.dev"
    headers {
      key: "api_key"
      value: "abcd"
    }
}
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})";
  planpb::Operator op;
  op.set_op_type(planpb::OTEL_EXPORT_SINK_OPERATOR);

  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(operator_pb_txt, op.mutable_otel_sink_op()));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(op.otel_sink_op());
  RowDescriptor input_rd({types::TIME64NS, types::FLOAT64});
  RowDescriptor output_rd({});

  std::shared_ptr<const grpc::AuthContext> auth_context;

  EXPECT_CALL(*mock_, Export(_, _, _))
      .Times(1)
      .WillRepeatedly(Invoke([&auth_context](const auto& ctx, const auto&, const auto&) {
        auth_context = ctx->auth_context();
        return grpc::Status::OK;
      }));

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  auto rb1 = RowBatchBuilder(input_rd, 1, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Time64NSValue>({10})
                 .AddColumn<types::Float64Value>({1.0})
                 .get();
  tester.ConsumeNext(rb1, 1, 0);

  EXPECT_EQ(url_, "otlp.px.dev");
}

struct TestCase {
  std::string name;
  std::string operator_proto;
  std::vector<std::string> incoming_rowbatches;
  std::vector<std::string> expected_otel_protos;
};

class OTelMetricsTest : public OTelExportSinkNodeTest,
                        public ::testing::WithParamInterface<TestCase> {};

TEST_P(OTelMetricsTest, process_data) {
  auto tc = GetParam();
  std::vector<otelmetricscollector::ExportMetricsServiceRequest> actual_protos(
      tc.expected_otel_protos.size());
  size_t i = 0;
  EXPECT_CALL(*mock_, Export(_, _, _))
      .Times(tc.expected_otel_protos.size())
      .WillRepeatedly(Invoke([&i, &actual_protos](const auto&, const auto& proto, const auto&) {
        actual_protos[i] = proto;
        ++i;
        return grpc::Status::OK;
      }));

  planpb::Operator op;
  op.set_op_type(planpb::OTEL_EXPORT_SINK_OPERATOR);

  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(tc.operator_proto, op.mutable_otel_sink_op()));
  auto plan_node = std::make_unique<plan::OTelExportSinkOperator>(1);
  auto s = plan_node->Init(op.otel_sink_op());

  // Load a RowBatch to get the Input RowDescriptor.
  table_store::schemapb::RowBatchData row_batch_proto;
  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(tc.incoming_rowbatches[0], &row_batch_proto));
  RowDescriptor input_rd = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie()->desc();
  RowDescriptor output_rd({});

  auto tester = exec::ExecNodeTester<OTelExportSinkNode, plan::OTelExportSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());
  for (const auto& rb_pb_txt : tc.incoming_rowbatches) {
    table_store::schemapb::RowBatchData row_batch_proto;
    EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(rb_pb_txt, &row_batch_proto));
    auto rb = RowBatch::FromProto(row_batch_proto).ConsumeValueOrDie();
    tester.ConsumeNext(*rb.get(), 1, 0);
  }

  for (size_t i = 0; i < tc.expected_otel_protos.size(); ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(tc.expected_otel_protos[i]));
  }
}

INSTANTIATE_TEST_SUITE_P(OTelMetrics, OTelMetricsTest,
                         ::testing::ValuesIn(std::vector<TestCase>{
                             {"summary",
                              R"pb(
metrics {
  name: "http.resp.latency"
  attributes {
    name: "http.method"
    column {
      column_type: STRING
      column_index: 1
    }
  }
  time_column_index: 0
  summary {
    count_column_index: 2
    sum_column_index: 3
    quantile_values {
      quantile: 0.5
      value_column_index: 4
    }
    quantile_values {
      quantile: 0.99
      value_column_index: 5
    }
  }
}
)pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { string_data { data: "GET" data: "POST" } }
cols { int64_data { data: 10 data: 100 } }
cols { float64_data { data: 100 data: 2000 } }
cols { float64_data { data: 15 data: 150 } }
cols { float64_data { data: 60 data: 600 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      summary {
        data_points {
          time_unix_nano: 10
          count: 10
          sum: 100
          quantile_values {
            quantile: 0.5
            value: 15
          }
          quantile_values {
            quantile: 0.99
            value: 60
          }
          attributes {
            key: "http.method"
            value {
              string_value: "GET"
            }
          }
        }
      }
    }
  }
}
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      summary {
        data_points {
          time_unix_nano: 11
          count: 100
          sum: 2000
          quantile_values {
            quantile: 0.5
            value: 150
          }
          quantile_values {
            quantile: 0.99
            value: 600
          }
          attributes {
            key: "http.method"
            value {
              string_value: "POST"
            }
          }
        }
      }
    }
  }
})pb"}},
                             {"gauge_float_and_attributes",
                              R"pb(
resource {
  attributes {
    name: "service.name"
    column {
      column_type: STRING
      column_index: 1
    }
  }
}
metrics {
  name: "http.resp.latency"
  attributes {
    name: "http.method"
    column {
      column_type: STRING
      column_index: 2
    }
  }
  time_column_index: 0
  gauge { float_column_index: 3 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { string_data { data: "pl/querybroker" data: "pl/metadata" } }
cols { string_data { data: "GET" data: "POST" } }
cols { float64_data { data: 15 data: 150 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/querybroker"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_double: 15
          attributes {
            key: "http.method"
            value {
              string_value: "GET"
            }
          }
        }
      }
    }
  }
}
resource_metrics {
  resource {
    attributes {
      key: "service.name"
      value {
        string_value: "pl/metadata"
      }
    }
  }
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_double: 150
          attributes {
            key: "http.method"
            value {
              string_value: "POST"
            }
          }
        }
      }
    }
  }
})pb"}},

                             {"gauge_int",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 data: 11 } }
cols { int64_data { data: 15 data: 150 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},

                             {"first_column_not_time_column",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 1
  gauge { int_column_index: 0 }
})pb",
                              {R"pb(
cols { int64_data { data: 15 data: 150 } }
cols { time64ns_data { data: 10 data: 11 } }
num_rows: 2
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
}
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},
                             {"description_and_unit",
                              R"pb(
metrics {
  name: "http.resp.latency"
  description: "tracks the response latency of http requests"
  unit: "ns"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { int64_data { data: 15  } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      description: "tracks the response latency of http requests"
      unit: "ns"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
})pb"}},
                             {"multi_batch",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { int64_data { data: 15 } }
num_rows: 1)pb",
                               R"pb(
cols { time64ns_data { data: 11 } }
cols { int64_data { data: 150 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
})pb",
                               R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      gauge {
        data_points {
          time_unix_nano: 11
          as_int: 150
        }
      }
    }
  }
})pb"}},
                             {"summary_optional_sum_column",
                              R"pb(
metrics {
  name: "http.resp.latency"
  time_column_index: 0
  summary {
    count_column_index: 1
    sum_column_index: -1
    quantile_values {
      quantile: 0.99
      value_column_index: 2
    }
  }
})pb",
                              {R"pb(
cols { time64ns_data { data: 10  } }
cols { int64_data { data: 10 } }
cols { float64_data { data: 600 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "http.resp.latency"
      summary {
        data_points {
          time_unix_nano: 10
          count: 10
          quantile_values {
            quantile: 0.99
            value: 600
          }
        }
      }
    }
  }
})pb"}},

                             {"many_metrics",
                              R"pb(
metrics {
  name: "metric1"
  time_column_index: 0
  gauge { int_column_index: 1 }
}
metrics {
  name: "metric2"
  time_column_index: 0
  gauge { int_column_index: 1 }
})pb",

                              {R"pb(
cols { time64ns_data { data: 10 } }
cols { int64_data { data: 15 } }
num_rows: 1
eow: true
eos: true)pb"},
                              {R"pb(
resource_metrics {
  resource {}
  instrumentation_library_metrics {
    metrics {
      name: "metric1"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
    metrics {
      name: "metric2"
      gauge {
        data_points {
          time_unix_nano: 10
          as_int: 15
        }
      }
    }
  }
})pb"}},
                         }),
                         [](const ::testing::TestParamInfo<TestCase>& info) {
                           return info.param.name;
                         });

}  // namespace exec
}  // namespace carnot
}  // namespace px
