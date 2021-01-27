#pragma once

#include <sole.hpp>
#include "src/api/public/uuidpb/uuid.pb.h"
#include "src/common/base/base.h"

namespace pl {

// TODO(zasgar): We need to optimize UUID to just send raw bytes instead of the string.
/**
 * Parses a proto message into sole::uuid.
 * @param uuid_proto
 * @return proto message
 */
inline StatusOr<sole::uuid> ParseUUID(const pl::uuidpb::UUID& uuid_proto) {
  if (uuid_proto.data().size() != 36) {
    return error::InvalidArgument("Malformed UUID: $0", uuid_proto.data());
  }
  return sole::rebuild(uuid_proto.data());
}

/**
 * Converts sole::uuid into a UUID proto.
 * @param uuid
 * @param uuid_proto
 */
inline void ToProto(const sole::uuid& uuid, pl::uuidpb::UUID* uuid_proto) {
  *(uuid_proto->mutable_data()) = uuid.str();
}

inline void ClearUUID(sole::uuid* uuid) {
  CHECK(uuid != nullptr);
  uuid->ab = 0;
  uuid->cd = 0;
}

}  // namespace pl

// Allow UUID to be logged.
namespace sole {
inline std::ostream& operator<<(std::ostream& os, const uuid& id) {
  os << id.str();
  return os;
}
}  // namespace sole
