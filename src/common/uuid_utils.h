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
StatusOr<sole::uuid> ParseUUID(const pl::utils::UUID& uuid_proto) {
  if (uuid_proto.data().size() != 36) {
    return error::InvalidArgument("Malformed UUID: $0", uuid_proto.data());
  }
  return sole::rebuild(uuid_proto.data());
}

/**
 * Converts a sole::uuid into a UUID proto.
 * @param uuid
 * @return the uuid proto.
 */
pl::utils::UUID ToProto(const sole::uuid& uuid) {
  pl::utils::UUID uuid_proto;
  *(uuid_proto.mutable_data()) = uuid.str();
  return uuid_proto;
}

}  // namespace pl
