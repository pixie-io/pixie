#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <type_traits>

#include "src/carnot/udf/udf_wrapper.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace udf {

using ::testing::ElementsAre;

class BasicUDTFOneCol : public UDTF<BasicUDTFOneCol> {
 public:
  static constexpr auto Executor() { return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg("some_int", types::DataType::INT64, "Int arg"),
                     UDTFArg("some_string", types::DataType::STRING, "String arg"));
  }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("out_str", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
  }

  Status Init(types::Int64Value init1, types::StringValue init2) {
    EXPECT_EQ(init1, 1337);
    EXPECT_EQ(init2, "abc");
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (idx++ < 2) {
      rw->Append<IndexOf("out_str")>("abc " + std::to_string(idx));
      return true;
    }
    return false;
  }

 private:
  int idx = 0;
};

TEST(BasicUDTFOneCol, can_run) {
  UDTFTraits<BasicUDTFOneCol> traits;
  constexpr auto init_args = traits.InitArgumentTypes();
  ASSERT_THAT(init_args, ElementsAre(types::DataType::INT64, types::DataType::STRING));
  constexpr auto output_rel = traits.OutputRelationTypes();
  ASSERT_THAT(output_rel, ElementsAre(types::DataType::STRING));

  UDTFWrapper<BasicUDTFOneCol> wrapper;
  PL_UNUSED(wrapper);
  types::Int64Value init1 = 1337;
  types::StringValue init2 = "abc";

  auto u = wrapper.Make({&init1, &init2}).ConsumeValueOrDie();

  arrow::StringBuilder string_builder(0);
  std::vector<arrow::ArrayBuilder*> outs{&string_builder};

  EXPECT_FALSE(wrapper.ExecBatchUpdate(u.get(), nullptr, 100, &outs));

  std::shared_ptr<arrow::StringArray> out;
  EXPECT_TRUE(string_builder.Finish(&out).ok());

  EXPECT_EQ(out->length(), 2);
  EXPECT_EQ(out->GetString(0), "abc 1");
  EXPECT_EQ(out->GetString(1), "abc 2");
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
