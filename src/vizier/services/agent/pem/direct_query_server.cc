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

// Note: stdlib + boringssl + rapidjson + absl includes stay at the top
// (not inside the `#ifndef` below) because cpplint's
// build/include_what_you_use scan doesn't follow preprocessor branches
// and would otherwise flag every type used in the feature body as
// "missing include". The disabled build pays a few KB of unused header
// parse cost; the .cc emits nothing for them.
#include <openssl/hmac.h>
#include <openssl/mem.h>
#include <openssl/sha.h>
#include <rapidjson/document.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
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
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace vizier {
namespace agent {

namespace {

constexpr char kBearerPrefixLower[] = "bearer ";
constexpr size_t kBearerPrefixLen = sizeof(kBearerPrefixLower) - 1;
constexpr char kExpectedAudience[] = "vizier";
constexpr char kExpectedIssuer[] = "PL";
// Service tokens carry "service" in the Scopes claim (NOT the subject — sub is
// the serviceID, e.g. "dx"). See GenerateJWTForService (claims.go) + jwt.go:56.
constexpr char kServiceScope[] = "service";

// cpp_jwt mints our outgoing service tokens (shared/manager/manager.cc), but we
// cannot use it to VERIFY here: HMACSign<>::verify (impl/algorithm.ipp) base64s
// through BIO_f_base64(). BoringSSL declares that in the public bio.h but
// implements it in decrepit/bio/base64_bio.c, which @boringssl//:crypto does not
// build — linking a jwt::decode(..., verify(true)) call fails with
// `undefined symbol: BIO_f_base64`. (Signing links because HMACSign<>::sign uses
// HMAC() plus cpp_jwt's header-only base64, no BIO.) Using the library for
// verification would mean patching the BoringSSL external to add a decrepit
// target; instead we parse the envelope here and HMAC with BoringSSL natively.
// Its base64url decoder needs no BIO, so we do reuse that below.

// stripBearerPrefix returns the token slice after a case-insensitive "Bearer "
// prefix, or an empty string if the prefix is missing. gRPC normalises metadata
// keys to lowercase but does NOT touch values; manager.cc:440 mints with a
// lowercase "bearer " prefix, but real-world clients may use "Bearer " (RFC 6750
// Title-case), so we accept both.
absl::string_view stripBearerPrefix(absl::string_view value) {
  if (value.size() < kBearerPrefixLen) {
    return {};
  }
  for (size_t i = 0; i < kBearerPrefixLen; ++i) {
    char c = value[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != kBearerPrefixLower[i]) return {};
  }
  return value.substr(kBearerPrefixLen);
}

// constantTimeEquals: BoringSSL's CRYPTO_memcmp, which is the library's own
// constant-time comparison — no hand-rolled crypto here. Length is compared
// first: the signature length is a function of the algorithm, not of the
// secret, so leaking "wrong length" leaks nothing about the key.
bool constantTimeEquals(absl::string_view a, absl::string_view b) {
  if (a.size() != b.size()) return false;
  return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// base64UrlDecode handles RFC 7515 base64url (no padding, '-' / '_' alphabet),
// delegating the transform to cpp_jwt's header-only decoder (the one part of
// that library that needs no BIO, so it links against our BoringSSL).
bool base64UrlDecode(absl::string_view in, std::string* out) {
  *out = jwt::base64_uri_decode(in.data(), in.size());
  return true;
}

// hmacSha256: BoringSSL HMAC over `data`, returns raw 32 bytes.
std::string hmacSha256(absl::string_view key, absl::string_view data) {
  uint8_t out[EVP_MAX_MD_SIZE];
  unsigned out_len = 0;
  const auto* res = HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
                         reinterpret_cast<const uint8_t*>(data.data()), data.size(), out, &out_len);
  if (res == nullptr) {
    return {};
  }
  return std::string(reinterpret_cast<const char*>(out), out_len);
}

// verifyHs256Jwt: parse <header>.<payload>.<signature>, check the header alg is
// HS256, verify the signature with BoringSSL HMAC, then validate the audience
// and expiry claims. Returns OK on success.
::grpc::Status verifyHs256Jwt(absl::string_view token, const std::string& signing_key) {
  // Split into 3 parts.
  std::vector<absl::string_view> parts = absl::StrSplit(token, '.');
  if (parts.size() != 3) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: malformed JWT");
  }
  // Verify HS256 alg in the header (refuse "alg":"none" forgeries).
  std::string header_json;
  if (!base64UrlDecode(parts[0], &header_json)) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: bad header b64");
  }
  rapidjson::Document header;
  if (header.Parse(header_json.c_str()).HasParseError() || !header.IsObject() ||
      !header.HasMember("alg") || !header["alg"].IsString() ||
      std::strcmp(header["alg"].GetString(), "HS256") != 0) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: unsupported JWT alg (HS256 only)");
  }
  // Verify the signature.
  std::string signing_input = std::string(parts[0].data(), parts[0].size()) + "." +
                              std::string(parts[1].data(), parts[1].size());
  std::string computed_mac = hmacSha256(signing_key, signing_input);
  if (computed_mac.empty()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: HMAC compute failed");
  }
  std::string signature;
  if (!base64UrlDecode(parts[2], &signature)) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: bad signature b64");
  }
  if (!constantTimeEquals(signature, computed_mac)) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: signature mismatch");
  }
  // Validate the payload claims (audience, expiry).
  std::string payload_json;
  if (!base64UrlDecode(parts[1], &payload_json)) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: bad payload b64");
  }
  rapidjson::Document payload;
  if (payload.Parse(payload_json.c_str()).HasParseError() || !payload.IsObject()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: payload not a JSON object");
  }
  // RFC 7519 §4.1.3 — `aud` may be a single string OR an array of strings. The
  // pixie-wide mint path (src/shared/services/utils/jwt.go:46) emits the array
  // form (`"aud":["vizier"]`), and kelvin / query-broker verifiers accept both;
  // we do the same so dx's live tokens authenticate against this verifier.
  if (!payload.HasMember("aud")) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: missing aud claim");
  }
  const auto& aud = payload["aud"];
  bool aud_ok = false;
  if (aud.IsString() && std::strcmp(aud.GetString(), kExpectedAudience) == 0) {
    aud_ok = true;
  } else if (aud.IsArray()) {
    for (const auto& v : aud.GetArray()) {
      if (v.IsString() && std::strcmp(v.GetString(), kExpectedAudience) == 0) {
        aud_ok = true;
        break;
      }
    }
  }
  if (!aud_ok) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: wrong audience (expected vizier)");
  }
  if (!payload.HasMember("iss") || !payload["iss"].IsString() ||
      std::strcmp(payload["iss"].GetString(), kExpectedIssuer) != 0) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: wrong iss (expected PL)");
  }
  // Require the "service" scope, NOT sub=="service". Pixie service tokens set
  // sub=<serviceID> (e.g. "dx") and put "service" in the Scopes claim — a
  // comma-joined string (GenerateJWTForService in claims.go + jwt.go:56). The
  // canonical verifier (jwt.go ParseToken) authenticates on signature+audience
  // and never asserts the subject; checking the scope rejects user/cluster
  // tokens while accepting any serviceID subject. (Previously this rejected
  // every real in-cluster caller with "wrong sub".)
  if (!payload.HasMember("Scopes") || !payload["Scopes"].IsString()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: missing Scopes claim");
  }
  bool has_service_scope = false;
  for (absl::string_view scope : absl::StrSplit(payload["Scopes"].GetString(), ',')) {
    if (scope == kServiceScope) {
      has_service_scope = true;
      break;
    }
  }
  if (!has_service_scope) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: token lacks the service scope");
  }
  if (!payload.HasMember("exp")) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: missing exp claim");
  }
  // Accept numeric exp (seconds since epoch) — matches RFC 7519 and what
  // manager.cc::GenerateServiceToken emits via jwt::jwt_object::add_claim.
  int64_t exp_secs = 0;
  if (payload["exp"].IsInt64()) {
    exp_secs = payload["exp"].GetInt64();
  } else if (payload["exp"].IsUint64()) {
    exp_secs = static_cast<int64_t>(payload["exp"].GetUint64());
  } else if (payload["exp"].IsDouble()) {
    exp_secs = static_cast<int64_t>(payload["exp"].GetDouble());
  } else {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: exp not numeric");
  }
  const auto now_secs = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  if (now_secs >= exp_secs) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, "direct-query: token expired");
  }
  return ::grpc::Status::OK;
}

}  // namespace

::grpc::Status AuthenticateRequest(::grpc::ServerContext* ctx, const std::string& jwt_signing_key) {
  if (jwt_signing_key.empty()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: signing key not configured");
  }
  const auto& md = ctx->client_metadata();
  auto it = md.find("authorization");
  if (it == md.end()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: missing authorization metadata");
  }
  absl::string_view raw(it->second.data(), it->second.size());
  absl::string_view token = stripBearerPrefix(raw);
  if (token.empty()) {
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: authorization is not a Bearer token");
  }
  auto status = verifyHs256Jwt(token, jwt_signing_key);
  if (!status.ok()) {
    VLOG(1) << "direct-query: " << status.error_message();
    // Collapse the specific error to a generic "invalid bearer token" on the
    // wire — peers don't need to know whether the signature or the claim
    // failed, only that they're unauthenticated. The VLOG above keeps the
    // diagnostic for the operator.
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "direct-query: invalid bearer token");
  }
  return ::grpc::Status::OK;
}

namespace {

// pixiePbTypeForCarnot translates a carnot/types DataType into the vizierpb
// column type that ExecuteScriptResponse.meta_data.relation expects. Mirrors
// standalone_pem/vizier_server.h:147-167.
::px::api::vizierpb::DataType pixiePbTypeForCarnot(::px::types::DataType t) {
  switch (t) {
    case ::px::types::BOOLEAN:
      return ::px::api::vizierpb::BOOLEAN;
    case ::px::types::INT64:
      return ::px::api::vizierpb::INT64;
    case ::px::types::UINT128:
      return ::px::api::vizierpb::UINT128;
    case ::px::types::FLOAT64:
      return ::px::api::vizierpb::FLOAT64;
    case ::px::types::STRING:
      return ::px::api::vizierpb::STRING;
    case ::px::types::TIME64NS:
      return ::px::api::vizierpb::TIME64NS;
    default:
      return ::px::api::vizierpb::DATA_TYPE_UNKNOWN;
  }
}

// emitSchemaResponses walks the compiled plan once and writes a meta_data-only
// ExecuteScriptResponse per GRPC_SINK_OPERATOR sink. The client uses these to
// learn output table names and column types before the data chunks arrive.
// Mirrors standalone_pem/vizier_server.h:132-173.
void emitSchemaResponses(const ::px::carnot::planpb::Plan& plan, const std::string& query_id,
                         ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer) {
  for (const auto& f : plan.nodes()) {
    for (const auto& n : f.nodes()) {
      if (n.op().op_type() != ::px::carnot::planpb::OperatorType::GRPC_SINK_OPERATOR) continue;
      const auto& sink = n.op().grpc_sink_op();
      if (!sink.has_output_table()) continue;
      ::px::api::vizierpb::ExecuteScriptResponse schema_resp;
      schema_resp.set_query_id(query_id);
      auto* metadata = schema_resp.mutable_meta_data();
      metadata->set_name(sink.output_table().table_name());
      metadata->set_id(sink.output_table().table_name());
      auto* rel = metadata->mutable_relation();
      for (int i = 0; i < sink.output_table().column_names().size(); ++i) {
        auto* col = rel->add_columns();
        col->set_column_name(sink.output_table().column_names()[i]);
        col->set_column_type(pixiePbTypeForCarnot(
            static_cast<::px::types::DataType>(sink.output_table().column_types()[i])));
      }
      writer->Write(schema_resp);
    }
  }
}

// drainSinkAndStream converts each accumulated TransferResultChunkRequest into
// an ExecuteScriptResponse and writes it to the gRPC stream. Mirrors
// standalone_pem/sink_server.h:60-105 but operates on already-collected
// chunks rather than a streaming consumer.
//
// Per-row column data + exec stats are copied by wire-format round-trip:
// carnotpb's and vizierpb's Column messages are bytewise identical (same oneof
// tags + field numbers for boolean/int64/uint128/time64ns/float64/string), the
// surrounding RowBatchData shares field numbers 1-4 (cols/num_rows/eow/eos),
// and QueryExecutionStats shares field 1 (timing) / 2 (bytes_processed) /
// 3 (records_processed). Wire-format roundtrip carries the data without a
// per-type switch; vizier-only RowBatchData.table_id (field 5) is set
// explicitly.
//
// **Don't write empty responses.** pxapi/results.go:142-143 returns
// "unimplemented type : internal error" when an ExecuteScriptResponse has
// neither meta_data, data.batch, data.encrypted_batch, nor data.execution_stats
// set. Carnot's sink emits chunks that are neither query_result nor
// execution_error (e.g. initiate_conn). Skip those instead of writing
// query_id-only frames.
void drainSinkAndStream(::px::carnot::exec::LocalGRPCResultSinkServer* result_server,
                        const std::string& query_id,
                        ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* writer) {
  for (const auto& chunk : result_server->raw_query_results()) {
    ::px::api::vizierpb::ExecuteScriptResponse resp;
    resp.set_query_id(query_id);
    bool has_payload = false;

    if (chunk.has_query_result() && chunk.query_result().has_row_batch()) {
      const auto& src = chunk.query_result().row_batch();
      auto* batch = resp.mutable_data()->mutable_batch();
      std::string buf;
      if (src.SerializeToString(&buf) && batch->ParseFromString(buf)) {
        batch->set_table_id(chunk.query_result().table_name());
      } else {
        // Roundtrip failed — should never happen on a well-formed payload,
        // fall back to the metadata-only shape so the client at least sees
        // the batch boundary.
        batch->set_table_id(chunk.query_result().table_name());
        batch->set_num_rows(src.num_rows());
        batch->set_eow(src.eow());
        batch->set_eos(src.eos());
      }
      has_payload = true;
    }

    if (chunk.has_execution_and_timing_info() &&
        chunk.execution_and_timing_info().has_execution_stats()) {
      const auto& src_stats = chunk.execution_and_timing_info().execution_stats();
      auto* dst_stats = resp.mutable_data()->mutable_execution_stats();
      std::string buf;
      (void)(src_stats.SerializeToString(&buf) && dst_stats->ParseFromString(buf));
      has_payload = true;
    }

    if (chunk.has_execution_error() && chunk.execution_error().err_code() != 0) {
      auto* status = resp.mutable_status();
      status->set_message(chunk.execution_error().msg());
      has_payload = true;
    }

    if (!has_payload) {
      // initiate_conn or any future variant we haven't mapped — pxapi rejects
      // payload-less ExecuteScriptResponses as ErrInternalUnImplementedType.
      // Skipping preserves stream OK.
      continue;
    }
    writer->Write(resp);
  }
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

  // Compile to inspect the plan + emit schema headers, mirroring
  // standalone_pem/vizier_server.h:121-173.
  auto compiler_state = engine_state_->CreateLocalExecutionCompilerState(0);
  auto plan_or = ::px::carnot::planner::compiler::Compiler().Compile(request->query_str(),
                                                                     compiler_state.get());
  if (!plan_or.ok()) {
    auto msg = absl::Substitute("direct-query: PxL compile failed ($0)", plan_or.msg());
    VLOG(1) << msg;
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, msg);
  }
  const auto plan = plan_or.ConsumeValueOrDie();
  emitSchemaResponses(plan, query_id_str, writer);

  // Reset the sink so we only see chunks for THIS query, then execute.
  // Synchronous: Carnot::ExecuteQuery blocks until the plan finishes (same as
  // standalone_pem + carnot_test).
  //
  // exec_mu_ guards the reset-execute-drain critical section. The sink's
  // accumulator is shared mutable state across ExecuteScript calls — without
  // the lock, a concurrent caller's ResetQueryResults could wipe another
  // caller's chunks mid-drain, or two callers' chunks could interleave in
  // a single sink. Holding from before reset to after drain serializes
  // queries at the sink boundary, matching standalone_pem's single-threaded
  // assumption. dx_daemon doesn't fan out per-PEM today, so contention is
  // expected to be low; ConcurrentQueries_AllSucceed in the test verifies
  // the contract under N parallel callers. CodeRabbit r3364645000.
  absl::MutexLock lk(&exec_mu_);
  result_server_->ResetQueryResults();
  auto exec_s = carnot_->ExecuteQuery(request->query_str(), query_id, ::px::CurrentTimeNS());
  if (!exec_s.ok()) {
    auto msg = absl::Substitute("direct-query: PxL execute failed ($0)", exec_s.msg());
    VLOG(1) << msg;
    return ::grpc::Status(::grpc::StatusCode::INTERNAL, msg);
  }
  drainSinkAndStream(result_server_, query_id_str, writer);
  return ::grpc::Status::OK;
}

}  // namespace agent
}  // namespace vizier
}  // namespace px

#else  // PX_PEM_DIRECT_QUERY_DISABLED

// Compile-out stubs. Linker-satisfying definitions of the two public
// surfaces with zero feature behaviour. Includes deliberately minimal —
// no openssl, no rapidjson, no carnot — so a "feature disabled" build
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
