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
// HS256 JWT verification matches the C++ mint pattern at
// src/vizier/services/agent/shared/manager/manager.cc.
// ExecuteScript execution ports the standalone_pem path against the live Carnot.

#include "src/vizier/services/agent/pem/direct_query_server.h"

// Note: stdlib + absl includes stay at the top
// (not inside the `#ifndef` below) because cpplint's
// build/include_what_you_use scan doesn't follow preprocessor branches
// and would otherwise flag every type used in the feature body as
// "missing include". The disabled build pays a few KB of unused header
// parse cost; the .cc emits nothing for them.

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <absl/strings/substitute.h>
#include <jwt/base64.hpp>
#include <sole.hpp>

// Compile-time kill switch. When PX_PEM_DIRECT_QUERY_DISABLED is defined
// (passed via the //src/vizier/services/agent/pem:direct_query=disabled
// config_setting in BUILD.bazel), the entire feature-bearing body of this
// file — JWT verifier, Carnot exec path — is EXCLUDED. The
// DirectQueryServer class still resolves at link time (so callers don't
// break) but every public function returns UNAUTHENTICATED/UNIMPLEMENTED.
// Operators get a binary with zero direct-query feature code. See
// DIRECT_QUERY_SECURITY.md "Disabling the feature".
#ifndef PX_PEM_DIRECT_QUERY_DISABLED

#include "src/carnot/carnot.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/engine_state.h"
#include "src/carnot/exec/local_grpc_result_server.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/services/jwt/service_token.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/vizier/services/agent/shared/vizier_results/result_conversion.h"

namespace px {
namespace vizier {
namespace agent {

namespace {

constexpr char kAuthorizationMetadataKey[] = "authorization";

}  // namespace

::grpc::Status AuthenticateRequest(::grpc::ServerContext* ctx, const std::string& jwt_signing_key) {
  if (jwt_signing_key.empty()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: signing key not configured");
  }
  const auto& md = ctx->client_metadata();
  const auto range = md.equal_range(kAuthorizationMetadataKey);
  if (range.first == range.second) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: missing authorization metadata");
  }
  // client_metadata() is a multimap. Two authorization headers is not a request
  // to guess about -- picking one arbitrarily would let a caller present a valid
  // token alongside whatever else it wanted.
  if (std::next(range.first) != range.second) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: duplicate authorization metadata");
  }
  const std::string_view raw(range.first->second.data(), range.first->second.size());
  const std::string_view token = ::px::services::StripBearerPrefix(raw);
  if (token.empty()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: authorization is not a Bearer token");
  }
  const auto s = ::px::services::VerifyServiceJWT(token, jwt_signing_key);
  if (!s.ok()) {
    VLOG(1) << "direct-query: rejecting bearer token: " << s.msg();
    // Collapse to a generic message on the wire -- peers don't need to know
    // which check failed, only that they're unauthenticated.
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: invalid bearer token");
  }
  return ::grpc::Status::OK;
}

namespace {

// kMaxOutputRowsPerTable caps how much a single direct query can materialize.
// Results are buffered in the sink before they are streamed out, and the PEM is
// a memory-capped daemonset sharing a node with the workload it observes, so an
// unbounded px.display(px.DataFrame(...)) over a wide window could OOM it.
constexpr int64_t kMaxOutputRowsPerTable = 10000;

// collectResponses converts each accumulated TransferResultChunkRequest into an
// ExecuteScriptResponse. It returns them rather than writing them so the caller
// can release exec_mu_ before touching the network.
std::vector<::px::api::vizierpb::ExecuteScriptResponse> collectResponses(
    ::px::carnot::exec::LocalGRPCResultSinkServer* result_server, const std::string& query_id) {
  std::vector<::px::api::vizierpb::ExecuteScriptResponse> out;
  for (const auto& chunk : result_server->raw_query_results()) {
    auto resp = ExecuteScriptResponseFromChunk(chunk, query_id);
    if (!resp.has_value()) {
      continue;
    }
    out.push_back(std::move(*resp));
  }
  return out;
}

}  // namespace

::grpc::Status DirectQueryServer::ExecuteScript(
    ::grpc::ServerContext* context, const ::px::api::vizierpb::ExecuteScriptRequest* request,
    ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer) {
  if (auto s = AuthenticateRequest(context, jwt_signing_key_); !s.ok()) {
    return s;
  }
  if (request->mutation()) {
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                          "direct-query: mutations are out of scope (read-only endpoint)");
  }
  // Defensive: any of carnot_/engine_state_/result_server_ being null at this
  // point means the operator deploy is misconfigured. Refuse rather than crash.
  if (carnot_ == nullptr || engine_state_ == nullptr || result_server_ == nullptr) {
    return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                          "direct-query: server not wired with a live Carnot");
  }
  const auto query_id = sole::uuid4();
  const std::string query_id_str = query_id.str();

  // One timestamp for both the schema compile below and the execute further
  // down. Carnot::ExecuteQuery compiles the script again internally, and a
  // script using relative times ("-5m") resolves against whatever it is given:
  // compiling the schema at a different instant than the data can advertise a
  // relation that doesn't describe what is emitted.
  const auto time_now = ::px::CurrentTimeNS();

  // Compile once up front to read the plan's sinks and emit schema headers
  // before any data.
  auto compiler_state =
      engine_state_->CreateLocalExecutionCompilerState(time_now, kMaxOutputRowsPerTable);
  auto plan_or = ::px::carnot::planner::compiler::Compiler().Compile(request->query_str(),
                                                                     compiler_state.get());
  if (!plan_or.ok()) {
    auto msg = absl::Substitute("direct-query: PxL compile failed ($0)", plan_or.msg());
    VLOG(1) << msg;
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, msg);
  }
  const auto plan = plan_or.ConsumeValueOrDie();
  EmitSchemaResponses(plan, query_id_str, writer);

  std::vector<::px::api::vizierpb::ExecuteScriptResponse> responses;
  {
    // exec_mu_ guards reset-execute-collect. The sink's accumulator is shared
    // mutable state across ExecuteScript calls: without the lock a concurrent
    // caller's ResetQueryResults could wipe another caller's chunks, or two
    // callers' chunks could interleave in one sink.
    //
    // The lock deliberately does NOT cover the writes below. Holding it across
    // writer->Write() would let one slow or stalled client block every other
    // direct query on the node for as long as it took to read.
    absl::MutexLock lk(&exec_mu_);
    result_server_->ResetQueryResults();
    // Synchronous: ExecuteQuery blocks until the plan finishes.
    auto exec_s = carnot_->ExecuteQuery(request->query_str(), query_id, time_now);
    if (!exec_s.ok()) {
      auto msg = absl::Substitute("direct-query: PxL execute failed ($0)", exec_s.msg());
      VLOG(1) << msg;
      return ::grpc::Status(::grpc::StatusCode::INTERNAL, msg);
    }
    responses = collectResponses(result_server_, query_id_str);
  }

  for (const auto& resp : responses) {
    if (context->IsCancelled()) {
      return ::grpc::Status(::grpc::StatusCode::CANCELLED, "direct-query: client cancelled");
    }
    // Write returns false once the stream is broken; continuing would spin
    // through the remaining batches for a client that is gone.
    if (!writer->Write(resp)) {
      VLOG(1) << "direct-query: client stream closed mid-response, abandoning query "
              << query_id_str;
      break;
    }
  }
  return ::grpc::Status::OK;
}

}  // namespace agent
}  // namespace vizier
}  // namespace px

#else  // PX_PEM_DIRECT_QUERY_DISABLED

// Compile-out stubs. Linker-satisfying definitions of the two public
// surfaces with zero feature behaviour. Includes deliberately minimal —
// no carnot — so a "feature disabled" build
// carries no direct-query attack surface in the binary.

namespace px {
namespace vizier {
namespace agent {

::grpc::Status AuthenticateRequest(::grpc::ServerContext*, const std::string&) {
  return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                        "direct-query: compiled out of this build "
                        "(PX_PEM_DIRECT_QUERY_DISABLED)");
}

::grpc::Status DirectQueryServer::ExecuteScript(
    ::grpc::ServerContext*, const ::px::api::vizierpb::ExecuteScriptRequest*,
    ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>*) {
  // Reference the unused private fields so -Wunused-private-field doesn't
  // flag the disabled build. The class signature is the same in both modes
  // (so callers see one type); we just don't drive any work from these
  // pointers in the disabled stub.
  (void)carnot_;
  (void)engine_state_;
  (void)result_server_;
  (void)jwt_signing_key_;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                        "direct-query: compiled out of this build "
                        "(PX_PEM_DIRECT_QUERY_DISABLED)");
}

}  // namespace agent
}  // namespace vizier
}  // namespace px

#endif  // PX_PEM_DIRECT_QUERY_DISABLED
