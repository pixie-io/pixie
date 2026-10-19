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

// Direct-query gRPC endpoint for the normal (metadata-connected) PEM.
//
// Serves api.vizierpb.VizierService.ExecuteScript directly on the PEM,
// authenticated via HS256 JWT, against the live node-local Carnot.
// The TDD contract lives in direct_query_server_test.cc and DIRECT_QUERY_CONTRACT.md.
//
// Reference impl ported from: src/experimental/standalone_pem/vizier_server.h
// (px::vizier::agent::VizierServer). Differences for the real PEM: reuse the live
// Carnot + metadata (no second engine), and REQUIRE a valid cluster service JWT.

#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <utility>  // std::move (used in DirectQueryServer ctor)

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

// AuthenticateRequest verifies the `authorization: Bearer <jwt>` metadata on an
// incoming call against `jwt_signing_key` (HS256, unexpired, vizier-audience).
// Returns OK iff the token is valid; UNAUTHENTICATED otherwise. A missing token
// MUST NOT fall through to execution. See DIRECT_QUERY_CONTRACT.md § Auth.
::grpc::Status AuthenticateRequest(::grpc::ServerContext* ctx, const std::string& jwt_signing_key);

// DirectQueryServer serves api.vizierpb.VizierService.ExecuteScript directly on the
// PEM, authenticated, against the live node-local Carnot. Construct with the PEM's
// already-running engine — do not create a new one.
//
// `result_server` is the sink Carnot is configured to push results into; the
// server polls it after `ExecuteQuery` returns and converts the accumulated
// `TransferResultChunkRequest`s into streamed `ExecuteScriptResponse`s. Used
// the same way standalone_pem's VizierServer does, except with the live PEM's
// Carnot wired in production (see Step 4) and a CarnotTest-style fixture in
// unit tests. Pass nullptr to disable the execution path (used by auth-only
// tests so they don't need to stand up a TableStore).
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

  // ExecuteScript: authenticate, then run the PxL on the local Carnot and stream
  // ExecuteScriptResponse rows. Mutations are out of scope (return UNIMPLEMENTED).
  ::grpc::Status ExecuteScript(
      ::grpc::ServerContext* context, const ::px::api::vizierpb::ExecuteScriptRequest* request,
      ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer) override;

 private:
  carnot::Carnot* carnot_;
  carnot::EngineState* engine_state_;
  carnot::exec::LocalGRPCResultSinkServer* result_server_;
  std::string jwt_signing_key_;

  // exec_mu_ serializes the ResetQueryResults / ExecuteQuery /
  // drainSinkAndStream critical section. The LocalGRPCResultSinkServer's
  // accumulator is shared mutable state: a concurrent ExecuteScript call
  // would clear or interleave another caller's chunks if not protected
  // (CodeRabbit r3364645000 catch). Per-instance (not file-scope) so
  // distinct DirectQueryServer instances in tests don't over-serialize
  // against each other.
  mutable absl::Mutex exec_mu_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
