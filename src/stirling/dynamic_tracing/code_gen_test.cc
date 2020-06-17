#include "src/stirling/dynamic_tracing/code_gen.h"

#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dynamictracingpb::ScalarType;
using ::pl::stirling::dynamictracingpb::Struct;
using ::pl::stirling::dynamictracingpb::ValueType;
using ::testing::StrEq;

TEST(GenStructTest, Output) {
  Struct st;
  st.set_name("socket_data_event_t");

  Struct::Field* field = nullptr;

  field = st.add_fields();
  field->set_name("i32");
  field->mutable_type()->set_scalar(ScalarType::INT32);

  field = st.add_fields();
  field->set_name("i64");
  field->mutable_type()->set_scalar(ScalarType::INT64);

  field = st.add_fields();
  field->set_name("double_val");
  field->mutable_type()->set_scalar(ScalarType::DOUBLE);

  field = st.add_fields();
  field->set_name("msg");
  field->mutable_type()->set_scalar(ScalarType::VOID_POINTER);

  field = st.add_fields();
  field->set_name("str");
  field->mutable_type()->set_scalar(ScalarType::STRING);

  field = st.add_fields();
  field->set_name("attr");
  field->mutable_type()->set_struct_type("attr_t");

  ASSERT_OK_AND_THAT(GenStruct(st, /*indent_size*/ 4), StrEq("struct socket_data_event_t {\n"
                                                             "    int32_t i32;\n"
                                                             "    int64_t i64;\n"
                                                             "    double double_val;\n"
                                                             "    void* msg;\n"
                                                             "    char* str;\n"
                                                             "    struct attr_t attr;\n"
                                                             "};\n"));
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
