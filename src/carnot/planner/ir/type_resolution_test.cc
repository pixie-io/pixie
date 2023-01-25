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

#include <gtest/gtest.h>

#include "src/carnot/funcs/funcs.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {

class TypeResolutionTest : public ASTVisitorTest {
 protected:
  void SetUpRegistryInfo() {
    auto func_registry = std::make_unique<udf::Registry>("func_registry");
    funcs::RegisterFuncsOrDie(func_registry.get());
    auto udf_proto = func_registry->ToProto();
    registry_info_ = std::make_unique<planner::RegistryInfo>();
    PX_CHECK_OK(registry_info_->Init(udf_proto));
  }

  void SetUp() override {
    ASTVisitorTest::SetUp();
    SetUpRegistryInfo();

    auto rel_map = std::make_unique<RelationMap>();
    rel_map->emplace("cpu", Relation({types::INT64, types::FLOAT64, types::FLOAT64, types::FLOAT64,
                                      types::UINT128, types::INT64},
                                     {"count", "cpu0", "cpu1", "cpu2", "upid", "bytes"},
                                     {types::ST_NONE, types::ST_PERCENT, types::ST_PERCENT,
                                      types::ST_PERCENT, types::ST_UPID, types::ST_BYTES}));

    compiler_state_ = std::make_unique<CompilerState>(
        std::move(rel_map), /* sensitive_columns */ SensitiveColumnMap{}, registry_info_.get(),
        /* time_now */ time_now,
        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
        /* redaction_options */ RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});
    cpu_type_ = ValueType::Create(types::FLOAT64, types::ST_PERCENT);
    upid_type_ = ValueType::Create(types::UINT128, types::ST_UPID);
    bytes_type_ = ValueType::Create(types::INT64, types::ST_BYTES);
  }

  int64_t time_now = 1552607213931245000;
  std::shared_ptr<ValueType> cpu_type_;
  std::shared_ptr<ValueType> upid_type_;
  std::shared_ptr<ValueType> bytes_type_;
};

TEST_F(TypeResolutionTest, mem_src_and_sink) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "upid"});
  auto mem_sink = MakeMemSink(mem_src, "out");

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(mem_sink, compiler_state_.get()));
  EXPECT_TRUE(mem_sink->is_type_resolved());

  auto mem_src_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  auto mem_sink_type = std::static_pointer_cast<TableType>(mem_sink->resolved_type());
  EXPECT_TableHasColumnWithType(mem_src_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_sink_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_src_type, "upid", upid_type_);
  EXPECT_TableHasColumnWithType(mem_sink_type, "upid", upid_type_);
}

TEST_F(TypeResolutionTest, mem_sink_with_out_cols) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto mem_sink = MakeMemSink(mem_src, "out", {"cpu0", "upid"});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(mem_sink, compiler_state_.get()));
  EXPECT_TRUE(mem_sink->is_type_resolved());

  auto mem_src_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  auto mem_sink_type = std::static_pointer_cast<TableType>(mem_sink->resolved_type());
  EXPECT_TableHasColumnWithType(mem_src_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_src_type, "cpu1", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_src_type, "upid", upid_type_);
  EXPECT_TableHasColumnWithType(mem_sink_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_sink_type, "upid", upid_type_);
  EXPECT_FALSE(mem_sink_type->HasColumn("cpu1"));
}

TEST_F(TypeResolutionTest, grpc_sink_with_out_cols) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto grpc_sink = MakeGRPCSink(mem_src, "out", {"cpu0", "upid"});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(grpc_sink, compiler_state_.get()));
  EXPECT_TRUE(grpc_sink->is_type_resolved());

  auto mem_src_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  auto grpc_sink_type = std::static_pointer_cast<TableType>(grpc_sink->resolved_type());
  EXPECT_TableHasColumnWithType(mem_src_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_src_type, "cpu1", cpu_type_);
  EXPECT_TableHasColumnWithType(mem_src_type, "upid", upid_type_);
  EXPECT_TableHasColumnWithType(grpc_sink_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(grpc_sink_type, "upid", upid_type_);
  EXPECT_FALSE(grpc_sink_type->HasColumn("cpu1"));
}

TEST_F(TypeResolutionTest, drop) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto drop = MakeDrop(mem_src, {"cpu1", "upid"});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(drop, compiler_state_.get()));
  EXPECT_TRUE(drop->is_type_resolved());

  auto drop_type = std::static_pointer_cast<TableType>(drop->resolved_type());

  EXPECT_TableHasColumnWithType(drop_type, "cpu0", cpu_type_);
  EXPECT_FALSE(drop_type->HasColumn("cpu1"));
  EXPECT_FALSE(drop_type->HasColumn("upid"));
}

TEST_F(TypeResolutionTest, agg_no_groups) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto mean_func = MakeMeanFunc(MakeColumn("cpu0", 0));
  auto agg = MakeBlockingAgg(mem_src, {}, {ColumnExpression("cpu_mean", mean_func)});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(agg, compiler_state_.get()));
  EXPECT_TRUE(agg->is_type_resolved());

  auto mem_src_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  auto agg_type = std::static_pointer_cast<TableType>(agg->resolved_type());

  EXPECT_TableHasColumnWithType(agg_type, "cpu_mean", cpu_type_);
  EXPECT_FALSE(agg_type->HasColumn("cpu0"));
  EXPECT_FALSE(agg_type->HasColumn("cpu1"));
  EXPECT_FALSE(agg_type->HasColumn("upid"));
}

TEST_F(TypeResolutionTest, agg_removes_semantics) {
  auto mem_src = MakeMemSource("cpu", {"cpu0"});
  auto count_func = MakeCountFunc(MakeColumn("cpu0", 0));
  auto agg = MakeBlockingAgg(mem_src, {}, {ColumnExpression("count", count_func)});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(agg, compiler_state_.get()));
  EXPECT_TRUE(agg->is_type_resolved());

  auto mem_src_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  auto agg_type = std::static_pointer_cast<TableType>(agg->resolved_type());

  EXPECT_TableHasColumnWithType(agg_type, "count", ValueType::Create(types::INT64, types::ST_NONE));
  EXPECT_FALSE(agg_type->HasColumn("cpu0"));
  EXPECT_FALSE(agg_type->HasColumn("cpu1"));
  EXPECT_FALSE(agg_type->HasColumn("upid"));
}

TEST_F(TypeResolutionTest, agg_with_groups) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto mean_func = MakeMeanFunc(MakeColumn("cpu0", 0));
  auto agg =
      MakeBlockingAgg(mem_src, {MakeColumn("upid", 0)}, {ColumnExpression("cpu_mean", mean_func)});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(agg, compiler_state_.get()));
  EXPECT_TRUE(agg->is_type_resolved());

  auto agg_type = std::static_pointer_cast<TableType>(agg->resolved_type());

  EXPECT_TableHasColumnWithType(agg_type, "cpu_mean", cpu_type_);
  EXPECT_TableHasColumnWithType(agg_type, "upid", upid_type_);
  EXPECT_FALSE(agg_type->HasColumn("cpu0"));
  EXPECT_FALSE(agg_type->HasColumn("cpu1"));
}

TEST_F(TypeResolutionTest, map_removes_semantics) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto add_func = MakeAddFunc(MakeColumn("cpu0", 0), MakeColumn("cpu1", 0));
  auto map = MakeMap(mem_src, {ColumnExpression("cpu_sum", add_func)});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(map, compiler_state_.get()));
  EXPECT_TRUE(map->is_type_resolved());

  auto map_type = std::static_pointer_cast<TableType>(map->resolved_type());

  auto cpu_type_no_semantics = ValueType::Create(types::FLOAT64, types::ST_NONE);

  // Addition of 2 percents doesn't have a registered semantic type output b/c it doesn't make
  // sense.
  EXPECT_TableHasColumnWithType(map_type, "cpu_sum", cpu_type_no_semantics);
  EXPECT_FALSE(map_type->HasColumn("cpu0"));
  EXPECT_FALSE(map_type->HasColumn("cpu1"));
  EXPECT_FALSE(map_type->HasColumn("upid"));
}

TEST_F(TypeResolutionTest, map_keeps_semantics) {
  auto mem_src = MakeMemSource("cpu", {"bytes", "upid"});
  auto add_func = MakeAddFunc(MakeColumn("bytes", 0), MakeColumn("bytes", 0));
  auto map = MakeMap(mem_src, {ColumnExpression("bytes_sum", add_func)});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(map, compiler_state_.get()));
  EXPECT_TRUE(map->is_type_resolved());

  auto map_type = std::static_pointer_cast<TableType>(map->resolved_type());

  EXPECT_TableHasColumnWithType(map_type, "bytes_sum", bytes_type_);
  EXPECT_FALSE(map_type->HasColumn("cpu0"));
  EXPECT_FALSE(map_type->HasColumn("cpu1"));
  EXPECT_FALSE(map_type->HasColumn("upid"));
}

TEST_F(TypeResolutionTest, union) {
  auto mem_src1 = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto mem_src2 = MakeMemSource("cpu", {"cpu0", "cpu1", "upid"});
  auto union_ = MakeUnion({mem_src1, mem_src2});

  ASSERT_OK(ResolveOperatorType(mem_src1, compiler_state_.get()));
  EXPECT_TRUE(mem_src1->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(mem_src2, compiler_state_.get()));
  EXPECT_TRUE(mem_src2->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(union_, compiler_state_.get()));
  EXPECT_TRUE(union_->is_type_resolved());

  auto union_type = std::static_pointer_cast<TableType>(union_->resolved_type());

  EXPECT_TableHasColumnWithType(union_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(union_type, "cpu1", cpu_type_);
  EXPECT_TableHasColumnWithType(union_type, "upid", upid_type_);
}

TEST_F(TypeResolutionTest, join) {
  auto mem_src1 = MakeMemSource("cpu", {"cpu0", "upid"});
  auto mem_src2 = MakeMemSource("cpu", {"cpu1", "upid"});
  auto join = MakeJoin({mem_src1, mem_src2}, "left", {MakeColumn("upid", 0)},
                       {MakeColumn("upid", 1)}, {"", "_right"});

  ASSERT_OK(ResolveOperatorType(mem_src1, compiler_state_.get()));
  EXPECT_TRUE(mem_src1->is_type_resolved());
  ASSERT_OK(ResolveOperatorType(mem_src2, compiler_state_.get()));
  EXPECT_TRUE(mem_src2->is_type_resolved());
  ASSERT_OK(join->UpdateOpAfterParentTypesResolved());
  ASSERT_OK(ResolveOperatorType(join, compiler_state_.get()));
  EXPECT_TRUE(join->is_type_resolved());

  auto join_type = std::static_pointer_cast<TableType>(join->resolved_type());

  EXPECT_TableHasColumnWithType(join_type, "cpu0", cpu_type_);
  EXPECT_TableHasColumnWithType(join_type, "cpu1", cpu_type_);
  EXPECT_TableHasColumnWithType(join_type, "upid", upid_type_);
  EXPECT_TableHasColumnWithType(join_type, "upid_right", upid_type_);
}

TEST_F(TypeResolutionTest, udtf_src) {
  udfspb::UDTFSourceSpec udtf_spec;
  udtf_spec.set_name("test_udtf");
  udtf_spec.set_executor(udfspb::UDTF_ALL_AGENTS);
  Relation rel({types::INT64, types::FLOAT64}, {"latency_ns", "cpu_usage"},
               {types::ST_NONE, types::ST_PERCENT});
  EXPECT_OK(rel.ToProto(udtf_spec.mutable_relation()));
  auto udtf_src = MakeUDTFSource(udtf_spec, {}, {});

  ASSERT_OK(ResolveOperatorType(udtf_src, compiler_state_.get()));
  EXPECT_TRUE(udtf_src->is_type_resolved());

  auto udtf_type = std::static_pointer_cast<TableType>(udtf_src->resolved_type());

  auto latency_type = ValueType::Create(types::INT64, types::ST_NONE);

  EXPECT_TableHasColumnWithType(udtf_type, "cpu_usage", cpu_type_);
  EXPECT_TableHasColumnWithType(udtf_type, "latency_ns", latency_type);
}

TEST_F(TypeResolutionTest, func_ir_no_types) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "upid"});
  auto func_ir = MakeMultFunc(MakeFloat(1.234), MakeColumn("cpu0", 0));
  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveExpressionType(func_ir, compiler_state_.get(), {mem_src->resolved_type()}));
  EXPECT_TRUE(func_ir->is_type_resolved());

  auto func_type = std::static_pointer_cast<ValueType>(func_ir->resolved_type());
  auto expected_type = ValueType::Create(types::FLOAT64, types::ST_NONE);
  EXPECT_EQ(*expected_type, *func_type);
}

TEST_F(TypeResolutionTest, column_ir) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "upid"});
  auto column_ir = MakeColumn("cpu0", 0);

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveExpressionType(column_ir, compiler_state_.get(), {mem_src->resolved_type()}));
  EXPECT_TRUE(column_ir->is_type_resolved());

  auto col_type = std::static_pointer_cast<ValueType>(column_ir->resolved_type());
  EXPECT_EQ(*cpu_type_, *col_type);
}

TEST_F(TypeResolutionTest, data_ir) {
  auto string_ir = MakeString("test_str");
  auto float_ir = MakeFloat(1.234);
  auto int_ir = MakeInt(1234);
  auto time_ir = MakeTime(1234);
  auto uint128_ir = MakeUInt128("39a0f8ca-36f2-4794-8609-35f98c7fc9fe");

  ASSERT_OK(ResolveExpressionType(string_ir, compiler_state_.get(), {}));
  ASSERT_OK(ResolveExpressionType(float_ir, compiler_state_.get(), {}));
  ASSERT_OK(ResolveExpressionType(int_ir, compiler_state_.get(), {}));
  ASSERT_OK(ResolveExpressionType(time_ir, compiler_state_.get(), {}));
  ASSERT_OK(ResolveExpressionType(uint128_ir, compiler_state_.get(), {}));

  auto string_type = std::static_pointer_cast<ValueType>(string_ir->resolved_type());
  auto float_type = std::static_pointer_cast<ValueType>(float_ir->resolved_type());
  auto int_type = std::static_pointer_cast<ValueType>(int_ir->resolved_type());
  auto time_type = std::static_pointer_cast<ValueType>(time_ir->resolved_type());
  auto uint128_type = std::static_pointer_cast<ValueType>(uint128_ir->resolved_type());

  EXPECT_EQ(*ValueType::Create(types::STRING, types::ST_NONE), *string_type);
  EXPECT_EQ(*ValueType::Create(types::FLOAT64, types::ST_NONE), *float_type);
  EXPECT_EQ(*ValueType::Create(types::INT64, types::ST_NONE), *int_type);
  EXPECT_EQ(*ValueType::Create(types::TIME64NS, types::ST_NONE), *time_type);
  EXPECT_EQ(*ValueType::Create(types::UINT128, types::ST_NONE), *uint128_type);
}

TEST_F(TypeResolutionTest, func_ir_init_args) {
  auto mem_src = MakeMemSource("cpu", {"cpu0", "upid"});
  // Pretend that mult has 1 init arg.
  ASSERT_OK(AddUDFToRegistry("mult", types::FLOAT64, {types::FLOAT64}, {types::FLOAT64}));
  auto func_ir = MakeMultFunc(MakeFloat(1.234), MakeColumn("cpu0", 0));

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  EXPECT_TRUE(mem_src->is_type_resolved());
  ASSERT_OK(ResolveExpressionType(func_ir, compiler_state_.get(), {mem_src->resolved_type()}));
  EXPECT_TRUE(func_ir->is_type_resolved());

  auto func_type = std::static_pointer_cast<ValueType>(func_ir->resolved_type());
  auto expected_type = ValueType::Create(types::FLOAT64, types::ST_NONE);
  EXPECT_EQ(*expected_type, *func_type);
}

TEST_F(TypeResolutionTest, join_ir_wrong_types) {
  auto mem_src1 = MakeMemSource("cpu", {"cpu0", "upid"});
  auto mem_src2 = MakeMemSource("cpu", {"cpu0", "upid"});
  auto rel = compiler_state_->relation_map()->at("cpu");
  auto join_ir =
      MakeJoin({mem_src1, mem_src2}, "inner", rel, rel, {"cpu0"}, {"upid"}, {"_x", "_y"});

  ASSERT_OK(ResolveOperatorType(mem_src1, compiler_state_.get()));
  ASSERT_OK(ResolveOperatorType(mem_src2, compiler_state_.get()));
  ASSERT_TRUE(mem_src1->is_type_resolved());
  ASSERT_TRUE(mem_src2->is_type_resolved());

  EXPECT_COMPILER_ERROR(
      ResolveOperatorType(join_ir, compiler_state_.get()),
      R"(join columns specified in `left_on` and `right_on` must have the same datatype.*["cpu0" (FLOAT64) vs "upid" (UINT128)])");
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
