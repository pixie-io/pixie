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

#include "src/common/uuid/uuid_utils.h"

#include "src/common/testing/testing.h"

namespace px {

TEST(ParseUUID, basic_test) {
  px::uuidpb::UUID uuid_pb;
  uuid_pb.set_high_bits(0xea8aa095697f49f1);
  uuid_pb.set_low_bits(0xb127d50e5b6e2645);
  ASSERT_OK_AND_ASSIGN(auto parsed, ParseUUID(uuid_pb));
  EXPECT_EQ(parsed.str(), "ea8aa095-697f-49f1-b127-d50e5b6e2645");
}

TEST(ToProto, uuid_basic) {
  auto uuid = sole::rebuild("ea8aa095-697f-49f1-b127-d50e5b6e2645");
  px::uuidpb::UUID uuid_proto;
  ToProto(uuid, &uuid_proto);
  EXPECT_EQ(0xea8aa095697f49f1, uuid_proto.high_bits());
  EXPECT_EQ(0xb127d50e5b6e2645, uuid_proto.low_bits());
}

TEST(UUIDUtils, regression_test) {
  for (int i = 0; i < 1000; i++) {
    auto uuid = sole::uuid4();
    px::uuidpb::UUID uuid_proto;
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

}  // namespace px
