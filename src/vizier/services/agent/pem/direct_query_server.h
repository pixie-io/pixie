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

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <utility>

#include <absl/synchronization/mutex.h>

#include "src/api/proto/vizierpb/vizierapi.grpc.pb.h"
#include "src/api/proto/vizierpb/vizierapi.pb.h"

namespace px {
namespace carnot {
class Carnot;
class EngineState;
namespace exec {
class LocalGRPCResultSinkServer;
}  // namespace exec
}  // namespace carnot

namespace vizier {
namespace agent {

// Verifies the `authorization: Bearer <jwt>` metadata against jwt_signing_key.
::grpc::Status AuthenticateRequest(::grpc::ServerContext* ctx, const std::string& jwt_signing_key);

// Direct-query gRPC endpoint for the normal (metadata-connected) PEM.
//
// Serves api.vizierpb.VizierService.ExecuteScript directly on the PEM,
// authenticated via HS256 JWT, against the live node-local Carnot.
class DirectQueryServer final : public api::vizierpb::VizierService::Service {
 public:
  DirectQueryServer() = delete;
  DirectQueryServer(carnot::Carnot* carnot, carnot::EngineState* engine_state,
                    carnot::exec::LocalGRPCResultSinkServer* result_server,
                    std::string jwt_signing_key)
      : carnot_(carnot),
        engine_state_(engine_state),
        result_server_(result_server),
        jwt_signing_key_(std::move(jwt_signing_key)) {}

  ::grpc::Status ExecuteScript(
      ::grpc::ServerContext* context, const ::px::api::vizierpb::ExecuteScriptRequest* request,
      ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer) override;

 private:
  carnot::Carnot* carnot_;
  carnot::EngineState* engine_state_;
  carnot::exec::LocalGRPCResultSinkServer* result_server_;
  std::string jwt_signing_key_;

  mutable absl::Mutex exec_mu_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
