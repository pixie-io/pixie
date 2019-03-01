#pragma once

#include <sole.hpp>
#include "src/common/error.h"
#include "src/common/statusor.h"
#include "src/utils/proto/utils.pb.h"

namespace pl {

// TODO(zasgar): We need to optimize UUID to just send raw bytes instead of the string.
/**
 * Parses a proto message into sole::uuid.
 * @param uuid_proto
 * @return proto message
 */
inline StatusOr<sole::uuid> ParseUUID(const pl::utils::UUID& uuid_proto) {
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
inline void ToProto(const sole::uuid& uuid, pl::utils::UUID* uuid_proto) {
  *(uuid_proto->mutable_data()) = uuid.str();
}

}  // namespace pl
