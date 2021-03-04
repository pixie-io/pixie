#include "src/common/uuid/uuid_utils.h"

#include "src/common/testing/testing.h"

namespace pl {

TEST(ParseUUID, deprecated_test) {
  pl::uuidpb::UUID uuid_pb;
  *(uuid_pb.mutable_deprecated_data()) = "ea8aa095-697f-49f1-b127-d50e5b6e2645";
  ASSERT_OK_AND_ASSIGN(auto parsed, ParseUUID(uuid_pb));
  EXPECT_EQ(parsed.str(), "ea8aa095-697f-49f1-b127-d50e5b6e2645");
}

TEST(ParseUUID, basic_test) {
  pl::uuidpb::UUID uuid_pb;
  uuid_pb.set_high_bits(0xea8aa095697f49f1);
  uuid_pb.set_low_bits(0xb127d50e5b6e2645);
  ASSERT_OK_AND_ASSIGN(auto parsed, ParseUUID(uuid_pb));
  EXPECT_EQ(parsed.str(), "ea8aa095-697f-49f1-b127-d50e5b6e2645");
}

TEST(ParseUUID, bad_input) {
  pl::uuidpb::UUID uuid_pb;
  // The 1 is removed from 4th segment b127.
  *(uuid_pb.mutable_deprecated_data()) = "ea8aa095-697f-49f1-b27-d50e5b6e2645";
  auto parsed = ParseUUID(uuid_pb);
  ASSERT_FALSE(parsed.ok());
  EXPECT_TRUE(error::IsInvalidArgument(parsed.status()));
}

TEST(ToProto, uuid_basic) {
  auto uuid = sole::rebuild("ea8aa095-697f-49f1-b127-d50e5b6e2645");
  pl::uuidpb::UUID uuid_proto;
  ToProto(uuid, &uuid_proto);
  EXPECT_EQ(0xea8aa095697f49f1, uuid_proto.high_bits());
  EXPECT_EQ(0xb127d50e5b6e2645, uuid_proto.low_bits());
}

TEST(UUIDUtils, regression_test) {
  for (int i = 0; i < 1000; i++) {
    auto uuid = sole::uuid4();
    pl::uuidpb::UUID uuid_proto;
    ToProto(uuid, &uuid_proto);
    ASSERT_OK_AND_ASSIGN(auto res, ParseUUID(uuid_proto));
    EXPECT_EQ(res.str(), uuid.str());
  }
}

TEST(UUIDUtils, clear_uuid_makes_uuid_zero) {
  auto uuid = sole::uuid4();
  ClearUUID(&uuid);
  EXPECT_EQ(0, uuid.ab);
  EXPECT_EQ(0, uuid.cd);
}

}  // namespace pl
