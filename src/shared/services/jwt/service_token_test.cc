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

#include "src/shared/services/jwt/service_token.h"

#include <chrono>
#include <string>
#include <vector>

#include <jwt/jwt.hpp>

#include "src/common/testing/testing.h"

namespace px {
namespace services {

constexpr char kTestSigningKey[] = "test-signing-key-do-not-use-in-prod";
constexpr char kWrongSigningKey[] = "a-different-key";

enum class TokenKind {
  kValid,
  kExpired,
  kAudAsArray,
  kWrongAud,
  kMissingExp,
  kAlgNone,
  kWrongIss,
  kWrongScope,
};

std::string MakeToken(const std::string& signing_key, TokenKind kind) {
  using std::chrono::seconds;
  using std::chrono::system_clock;
  const auto now = system_clock::now();
  // Well outside the verifier's default 30s leeway in both directions.
  const auto exp_offset = (kind == TokenKind::kExpired) ? seconds{-600} : seconds{600};

  if (kind == TokenKind::kAlgNone) {
    // cpp-jwt won't emit alg:"none", so hand-craft the forgery.
    // {"alg":"none","typ":"JWT"} . {"aud":["vizier"],"exp":4102444800} . <empty>
    return "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0."
           "eyJhdWQiOlsidml6aWVyIl0sImV4cCI6NDEwMjQ0NDgwMH0.";
  }

  ::jwt::jwt_object obj{::jwt::params::algorithm("HS256")};
  obj.add_claim("iss", kind == TokenKind::kWrongIss ? "not-PL" : "PL");
  switch (kind) {
    case TokenKind::kAudAsArray:
      obj.add_claim("aud", std::vector<std::string>{"vizier"});
      break;
    case TokenKind::kWrongAud:
      obj.add_claim("aud", std::string("wrong-service"));
      break;
    default:
      obj.add_claim("aud", std::string("vizier"));
  }
  obj.add_claim("iat", now);
  obj.add_claim("nbf", now - std::chrono::seconds{600});
  if (kind != TokenKind::kMissingExp) {
    obj.add_claim("exp", now + exp_offset);
  }
  // sub is the serviceID, which the verifier must not assert on.
  obj.add_claim("sub", "dx");
  obj.add_claim("Scopes", kind == TokenKind::kWrongScope ? "user" : "user,service");
  obj.secret(signing_key);
  return obj.signature();
}

TEST(VerifyServiceJWT, ValidTokenAccepted) {
  EXPECT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), kTestSigningKey));
}

TEST(VerifyServiceJWT, AudAsArrayAccepted) {
  EXPECT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kAudAsArray), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongKeyRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), kWrongSigningKey));
}

TEST(VerifyServiceJWT, TamperedPayloadRejected) {
  auto tok = MakeToken(kTestSigningKey, TokenKind::kValid);
  const auto second_dot = tok.find('.', tok.find('.') + 1);
  tok[second_dot - 1] = (tok[second_dot - 1] == 'A') ? 'B' : 'A';
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kTestSigningKey));
}

TEST(VerifyServiceJWT, ExpiredRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kExpired), kTestSigningKey));
}

// cpp-jwt only validates exp when the claim is present.
TEST(VerifyServiceJWT, MissingExpRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kMissingExp), kTestSigningKey));
}

TEST(VerifyServiceJWT, AlgNoneRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kAlgNone), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongAudRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kWrongAud), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongIssRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kWrongIss), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongScopeRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kWrongScope), kTestSigningKey));
}

TEST(VerifyServiceJWT, MalformedTokenRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT("not-a-jwt-at-all", kTestSigningKey));
  EXPECT_NOT_OK(VerifyServiceJWT("", kTestSigningKey));
}

// cpp-jwt reads `alg` with .get<std::string>() without checking its type, which
// throws past the error_code. Unguarded, this terminates the process.
TEST(VerifyServiceJWT, NonStringAlgRejectedWithoutThrowing) {
  // {"alg":123,"typ":"JWT"} . {"aud":"vizier","exp":4102444800} . <sig>
  EXPECT_NOT_OK(
      VerifyServiceJWT("eyJhbGciOjEyMywidHlwIjoiSldUIn0."
                       "eyJhdWQiOiJ2aXppZXIiLCJleHAiOjQxMDI0NDQ4MDB9.AAAA",
                       kTestSigningKey));
}

// A misconfigured verifier must fail closed.
TEST(VerifyServiceJWT, EmptySigningKeyRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), ""));
}

TEST(VerifyServiceJWT, LeewayAllowsSmallClockSkew) {
  using std::chrono::seconds;
  ::jwt::jwt_object obj{::jwt::params::algorithm("HS256")};
  obj.add_claim("iss", "PL");
  obj.add_claim("aud", std::string("vizier"));
  obj.add_claim("Scopes", "service");
  obj.add_claim("exp", std::chrono::system_clock::now() - seconds{5});
  obj.secret(kTestSigningKey);
  const auto tok = obj.signature();

  ServiceJWTOptions no_leeway;
  no_leeway.leeway = seconds{0};
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kTestSigningKey, no_leeway));
  EXPECT_OK(VerifyServiceJWT(tok, kTestSigningKey, ServiceJWTOptions{}));
}

TEST(GenerateServiceToken, RoundTripsThroughVerify) {
  ASSERT_OK_AND_ASSIGN(const auto tok, GenerateServiceToken(kTestSigningKey, "pem"));
  EXPECT_OK(VerifyServiceJWT(tok, kTestSigningKey));
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kWrongSigningKey));
}

TEST(GenerateServiceToken, EmptyKeyReturnsError) {
  const auto s = GenerateServiceToken("", "pem").status();
  EXPECT_TRUE(error::IsInvalidArgument(s)) << s.msg();
}

TEST(StripBearerPrefix, AcceptsBothCasingsAndRejectsOtherSchemes) {
  EXPECT_EQ("tok", StripBearerPrefix("bearer tok"));
  EXPECT_EQ("tok", StripBearerPrefix("Bearer tok"));
  EXPECT_EQ("", StripBearerPrefix("Token tok"));
  EXPECT_EQ("", StripBearerPrefix("tok"));
  EXPECT_EQ("", StripBearerPrefix("bearer "));
}

}  // namespace services
}  // namespace px
