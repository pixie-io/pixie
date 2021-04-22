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

#pragma once

#include <sole.hpp>
#include "src/api/proto/uuidpb/uuid.pb.h"
#include "src/common/base/base.h"

namespace px {

/**
 * Parses a proto message into sole::uuid.
 * @param uuid_proto
 * @return proto message
 */
inline StatusOr<sole::uuid> ParseUUID(const px::uuidpb::UUID& uuid_proto) {
  return sole::rebuild(uuid_proto.high_bits(), uuid_proto.low_bits());
}

/**
 * Converts sole::uuid into a UUID proto.
 * @param uuid
 * @param uuid_proto
 */
inline void ToProto(const sole::uuid& uuid, px::uuidpb::UUID* uuid_proto) {
  uuid_proto->set_high_bits(uuid.ab);
  uuid_proto->set_low_bits(uuid.cd);
}

inline void ClearUUID(sole::uuid* uuid) {
  CHECK(uuid != nullptr);
  uuid->ab = 0;
  uuid->cd = 0;
}

}  // namespace px

// Allow UUID to be logged.
namespace sole {
inline std::ostream& operator<<(std::ostream& os, const uuid& id) {
  os << id.str();
  return os;
}
}  // namespace sole
