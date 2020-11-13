#include <gtest/gtest.h>

#include "src/shared/types/proto/wrapper/types_pb_wrapper.h"

TEST(SemanticType, MagicEnumBehavior) {
  // A semantic type with a low numeric value should always resolve.
  EXPECT_EQ(magic_enum::enum_name(pl::types::ST_NONE), "ST_NONE");

  // Make sure the magic_enum::customize is kicking in,
  // which increases the max enum value that is printable.
  EXPECT_EQ(magic_enum::enum_name(pl::types::ST_HTTP_RESP_MESSAGE), "ST_HTTP_RESP_MESSAGE");

  // Make sure that calling enum_name on a large semantic type does not cause any issues
  // other than not printing out the name.
  EXPECT_EQ(magic_enum::enum_name(pl::types::SemanticType_INT_MAX_SENTINEL_DO_NOT_USE_), "");
}
