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

// Translation from what Carnot produces (carnotpb) to what an ExecuteScript
// caller consumes (api.vizierpb).
//
// Anything serving VizierService::ExecuteScript from a node-local Carnot needs
// this translation.

#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/sync_stream.h>  // grpc::ServerWriter
#include <optional>
#include <string_view>

#include "src/api/proto/vizierpb/vizierapi.pb.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/table_store/schemapb/schema.pb.h"

namespace px {
namespace vizier {
namespace agent {

::px::api::vizierpb::DataType VizierDataTypeFromCarnot(::px::types::DataType t);

void EmitSchemaResponses(const ::px::carnot::planpb::Plan& plan, std::string_view query_id,
                         ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer);

std::optional<::px::api::vizierpb::ExecuteScriptResponse> ExecuteScriptResponseFromChunk(
    const ::px::carnotpb::TransferResultChunkRequest& chunk, std::string_view query_id);

}  // namespace agent
}  // namespace vizier
}  // namespace px
