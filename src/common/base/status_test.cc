#include <gtest/gtest.h>
#include <iostream>

#include "src/common/base/status.h"

namespace pl {

TEST(Status, Default) {
  Status status;
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(status, Status::OK());
  EXPECT_EQ(status.code(), pl::statuspb::OK);
}

TEST(Status, EqCopy) {
  Status a(pl::statuspb::UNKNOWN, "Badness");
  Status b = a;

  ASSERT_EQ(a, b);
}

TEST(Status, EqDiffCode) {
  Status a(pl::statuspb::UNKNOWN, "Badness");
  Status b(pl::statuspb::CANCELLED, "Badness");

  ASSERT_NE(a, b);
}

Status MacroTestFn(const Status& s) {
  PL_RETURN_IF_ERROR(s);
  return Status::OK();
}

TEST(Status, pl_return_if_error_test) {
  EXPECT_EQ(Status::OK(), MacroTestFn(Status::OK()));

  auto err_status = Status(pl::statuspb::UNKNOWN, "an error");
  EXPECT_EQ(err_status, MacroTestFn(err_status));

  // Check to make sure value to macro is used only once.
  int call_count = 0;
  auto fn = [&]() -> Status {
    call_count++;
    return Status::OK();
  };
  auto test_fn = [&]() -> Status {
    PL_RETURN_IF_ERROR(fn());
    return Status::OK();
  };
  EXPECT_OK(test_fn());
  EXPECT_EQ(1, call_count);
}

TEST(Status, to_proto) {
  Status s1(pl::statuspb::UNKNOWN, "error 1");
  auto pb1 = s1.ToProto();
  EXPECT_EQ(pl::statuspb::UNKNOWN, pb1.err_code());
  EXPECT_EQ("error 1", pb1.msg());

  Status s2(pl::statuspb::INVALID_ARGUMENT, "error 2");
  auto pb2 = s2.ToProto();
  EXPECT_EQ(pl::statuspb::INVALID_ARGUMENT, pb2.err_code());
  EXPECT_EQ("error 2", pb2.msg());

  pl::statuspb::Status status_proto;
  s2.ToProto(&status_proto);
  EXPECT_EQ(s2, Status(status_proto));
}

TEST(StatusAdapter, from_proto) {
  Status s1(pl::statuspb::UNKNOWN, "error 1");
  auto pb1 = s1.ToProto();
  EXPECT_EQ(s1, StatusAdapter(pb1));
}

TEST(StatusAdapter, from_proto_without_error) {
  auto pb1 = Status::OK().ToProto();
  std::cout << pb1.DebugString() << std::endl;
  EXPECT_TRUE(Status::OK() == StatusAdapter(pb1));
}

}  // namespace pl
