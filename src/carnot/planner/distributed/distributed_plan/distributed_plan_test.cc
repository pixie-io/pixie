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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/testing/protobuf.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using ::px::testing::proto::EqualsProto;
using ::px::testing::proto::Partially;

using DistributedPlanTest = ASTVisitorTest;

constexpr char kIRProto[] = R"proto(
qb_address_to_plan {
  key: "agent"
  value {
    dag {
      nodes {
        id: 1
      }
    }
    nodes {
      id: 1
      dag {
        nodes {
          sorted_children: 1
        }
        nodes {
          id: 1
          sorted_parents: 0
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table"
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
          }
        }
      }
      nodes {
        id: 1
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "agent"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
            column_semantic_types: ST_NONE
            column_semantic_types: ST_NONE
            column_semantic_types: ST_NONE
            column_semantic_types: ST_NONE
          }
        }
      }
    }
    execution_status_destinations {
      grpc_address: "1111"
      ssl_targetname: "kelvin.pl.svc"
    }
  }
}
qb_address_to_plan {
  key: "kelvin"
  value {
    plan_options {
      analyze: true
      explain: true
    }
    dag {
      nodes {
        id: 1
      }
    }
    nodes {
      id: 1
      dag {
        nodes {
          sorted_children: 1
        }
        nodes {
          id: 1
          sorted_parents: 0
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table"
            column_idxs: 0
            column_idxs: 1
            column_idxs: 2
            column_idxs: 3
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
          }
        }
      }
      nodes {
        id: 1
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "kelvin"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
            column_semantic_types: ST_NONE
            column_semantic_types: ST_NONE
            column_semantic_types: ST_NONE
            column_semantic_types: ST_NONE
          }
        }
      }
    }
    execution_status_destinations {
      grpc_address: "qb"
      ssl_targetname: "qb:51300"
    }
  }
}
qb_address_to_dag_id {
  key: "agent"
  value: 0
}
qb_address_to_dag_id {
  key: "kelvin"
  value: 1
}
dag {
  nodes {
    sorted_children: 1
  }
  nodes {
    id: 1
    sorted_parents: 0
  }
}
)proto";

TEST_F(DistributedPlanTest, construction_test) {
  auto physical_plan = std::make_unique<DistributedPlan>();
  distributedpb::DistributedState physical_state =
      LoadDistributedStatePb(kOneAgentDistributedState);
  std::unordered_map<int64_t, distributedpb::CarnotInfo> carnot_id_to_carnot_info_map;
  for (int64_t i = 0; i < physical_state.carnot_info_size(); ++i) {
    distributedpb::CarnotInfo carnot_info = physical_state.carnot_info()[i];
    auto carnot = physical_plan->AddCarnot(carnot_info).ConsumeValueOrDie();
    carnot_id_to_carnot_info_map.emplace(carnot, carnot_info);
  }

  for (const auto& [carnot_id, carnot_info] : carnot_id_to_carnot_info_map) {
    CarnotInstance* carnot_instance = physical_plan->Get(carnot_id);
    EXPECT_THAT(carnot_instance->carnot_info(), EqualsProto(carnot_info.DebugString()));
    EXPECT_THAT(carnot_instance->QueryBrokerAddress(), carnot_info.query_broker_address());
    auto new_graph = std::make_shared<IR>();
    SwapGraphBeingBuilt(new_graph);
    auto mem_source = MakeMemSource(MakeRelation());
    compiler_state_->relation_map()->emplace("table", MakeRelation());
    MakeMemSink(mem_source, carnot_instance->QueryBrokerAddress());

    compiler::ResolveTypesRule rule(compiler_state_.get());
    ASSERT_OK(rule.Execute(graph.get()));

    auto clone_uptr = new_graph->Clone().ConsumeValueOrDie();
    carnot_instance->AddPlan(clone_uptr.get());
    physical_plan->AddPlan(std::move(clone_uptr));

    auto carnot_plan_proto = carnot_instance->PlanProto().ConsumeValueOrDie();
  }

  physical_plan->SetExecutionCompleteAddress("qb", "qb:51300");

  physical_plan->AddEdge(physical_plan->Get(0), physical_plan->Get(1));

  planpb::PlanOptions plan_opts;
  plan_opts.set_analyze(true);
  plan_opts.set_explain(true);
  physical_plan->SetPlanOptions(plan_opts);

  auto physical_plan_proto = physical_plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(physical_plan_proto, Partially(EqualsProto(kIRProto)));
}

}  // namespace distributed

}  // namespace planner
}  // namespace carnot
}  // namespace px
