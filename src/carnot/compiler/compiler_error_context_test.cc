#include <gtest/gtest.h>

#include "absl/strings/str_format.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/common/base/proto/status.pb.h"
#include "src/common/base/status.h"

namespace pl {
namespace carnot {
namespace compiler {

TEST(CompilerErrorContextStatus, Default) {
  int64_t line = 10;
  int64_t column = 12;
  int64_t num_errors = 4;
  std::string message = "There's an error here.";
  compilerpb::CompilerErrorGroup errorgroup_in, errorgroup_out;
  for (int64_t i = 0; i < num_errors; i++) {
    compilerpb::CompilerError* error_parent = errorgroup_in.add_errors();
    error_parent->set_type(compilerpb::LINECOL);
    compilerpb::LineColError* error = error_parent->mutable_line_col_error();

    error->set_line(line + i);
    error->set_column(column + i);
    error->set_message(absl::StrFormat("msg: %s, idx: %d", message, i));
  }
  Status status(pl::statuspb::INVALID_ARGUMENT, "Issue",
                std::make_unique<compilerpb::CompilerErrorGroup>(errorgroup_in));

  pl::statuspb::Status status_pb = status.ToProto();
  ASSERT_TRUE(status_pb.context().Is<compilerpb::CompilerErrorGroup>());

  status_pb.context().UnpackTo(&errorgroup_out);
  EXPECT_EQ(errorgroup_in.DebugString(), errorgroup_out.DebugString());
  for (int64_t i = 0; i < errorgroup_in.errors_size(); i++) {
    auto error_parent_out = errorgroup_in.errors(i);
    ASSERT_EQ(error_parent_out.type(), compilerpb::LINECOL);
    auto error_out = error_parent_out.line_col_error();
    EXPECT_EQ(error_out.line(), line + i);
    EXPECT_EQ(error_out.column(), column + i);
    EXPECT_EQ(error_out.message(), absl::StrFormat("msg: %s, idx: %d", message, i));
  }
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
