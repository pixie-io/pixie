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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/decoder/packet_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

StatusOr<JoinGroupProtocol> PacketDecoder::ExtractJoinGroupProtocol() {
  JoinGroupProtocol r;
  PX_ASSIGN_OR_RETURN(r.protocol, ExtractString());
  PX_RETURN_IF_ERROR(/* metadata */ ExtractBytes());
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<JoinGroupMember> PacketDecoder::ExtractJoinGroupMember() {
  JoinGroupMember r;
  PX_ASSIGN_OR_RETURN(r.member_id, ExtractString());
  if (api_version_ >= 5) {
    PX_ASSIGN_OR_RETURN(r.group_instance_id, ExtractNullableString());
  }
  PX_RETURN_IF_ERROR(/* metadata */ ExtractBytes());
  PX_RETURN_IF_ERROR(/* tag_section */ ExtractTagSection());
  return r;
}

StatusOr<JoinGroupReq> PacketDecoder::ExtractJoinGroupReq() {
  JoinGroupReq r;
  PX_ASSIGN_OR_RETURN(r.group_id, ExtractString());

  PX_ASSIGN_OR_RETURN(r.session_timeout_ms, ExtractInt32());
  if (api_version_ >= 1) {
    PX_ASSIGN_OR_RETURN(r.rebalance_timeout_ms, ExtractInt32());
  }

  PX_ASSIGN_OR_RETURN(r.member_id, ExtractString());

  if (api_version_ >= 5) {
    PX_ASSIGN_OR_RETURN(r.group_instance_id, ExtractNullableString());
  }
  PX_ASSIGN_OR_RETURN(r.protocol_type, ExtractString());
  PX_ASSIGN_OR_RETURN(r.protocols, ExtractArray(&PacketDecoder::ExtractJoinGroupProtocol));

  // Reason was added in api_version >= 8 (nullable string). Consume it so any trailing
  // tag section stays aligned; it is currently not surfaced in the trace.
  if (api_version_ >= 8) {
    PX_RETURN_IF_ERROR(/* reason */ ExtractNullableString());
  }

  return r;
}

StatusOr<JoinGroupResp> PacketDecoder::ExtractJoinGroupResp() {
  JoinGroupResp r;
  if (api_version_ >= 2) {
    PX_ASSIGN_OR_RETURN(r.throttle_time_ms, ExtractInt32());
  }
  PX_ASSIGN_OR_RETURN(r.error_code, ExtractInt16());
  PX_ASSIGN_OR_RETURN(r.generation_id, ExtractInt32());
  if (api_version_ >= 7) {
    PX_ASSIGN_OR_RETURN(r.protocol_type, ExtractNullableString());
    PX_ASSIGN_OR_RETURN(r.protocol_name, ExtractNullableString());
  } else {
    PX_ASSIGN_OR_RETURN(r.protocol_type, ExtractString());
    PX_ASSIGN_OR_RETURN(r.protocol_name, ExtractString());
  }
  // SkipAssignment was added in api_version >= 9 (bool). Consume it so the trailing fields
  // stay aligned; it is currently not surfaced in the trace.
  if (api_version_ >= 9) {
    PX_RETURN_IF_ERROR(/* skip_assignment */ ExtractBool());
  }
  PX_ASSIGN_OR_RETURN(r.leader, ExtractString());
  PX_ASSIGN_OR_RETURN(r.member_id, ExtractString());
  PX_ASSIGN_OR_RETURN(r.members, ExtractArray(&PacketDecoder::ExtractJoinGroupMember));
  return r;
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
