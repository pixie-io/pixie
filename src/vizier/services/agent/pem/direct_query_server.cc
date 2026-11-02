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

#include "src/vizier/services/agent/pem/direct_query_server.h"

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
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: invalid bearer token");
  }
  return ::grpc::Status::OK;
}

namespace {

constexpr int64_t kMaxOutputRowsPerTable = 10000;

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
  if (carnot_ == nullptr || engine_state_ == nullptr || result_server_ == nullptr) {
    return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION,
                          "direct-query: server not wired with a live Carnot");
  }
  const auto query_id = sole::uuid4();
  const std::string query_id_str = query_id.str();

  const auto time_now = ::px::CurrentTimeNS();

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
    absl::MutexLock lk(&exec_mu_);
    result_server_->ResetQueryResults();
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
