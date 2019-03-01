#include "src/common/uuid_utils.h"
#include <gtest/gtest.h>

namespace pl {

TEST(ParseUUID, basic_test) {
  pl::utils::UUID uuid_pb;
  *(uuid_pb.mutable_data()) = "ea8aa095-697f-49f1-b127-d50e5b6e2645";
  auto parsed = ParseUUID(uuid_pb);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed.ConsumeValueOrDie().str(), "ea8aa095-697f-49f1-b127-d50e5b6e2645");
}

TEST(ParseUUID, bad_input) {
  pl::utils::UUID uuid_pb;
  // The 1 is removed from 4th segment b127.
  *(uuid_pb.mutable_data()) = "ea8aa095-697f-49f1-b27-d50e5b6e2645";
  auto parsed = ParseUUID(uuid_pb);
  ASSERT_FALSE(parsed.ok());
  EXPECT_TRUE(error::IsInvalidArgument(parsed.status()));
}

TEST(ToProto, uuid_basic) {
  auto uuid = sole::rebuild("ea8aa095-697f-49f1-b127-d50e5b6e2645");
  pl::utils::UUID uuid_proto;
  ToProto(uuid, &uuid_proto);
  EXPECT_EQ("ea8aa095-697f-49f1-b127-d50e5b6e2645", uuid_proto.data());
}

TEST(UUIDUtils, regression_test) {
  for (int i = 0; i < 1000; i++) {
    auto uuid = sole::uuid4();
    pl::utils::UUID uuid_proto;
    ToProto(uuid, &uuid_proto);
    auto res = ParseUUID(uuid_proto);
    ASSERT_TRUE(res.ok());
    EXPECT_EQ(res.ConsumeValueOrDie().str(), uuid.str());
  }
}

}  // namespace pl
