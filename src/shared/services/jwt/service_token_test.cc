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

// TokenKind drives MakeToken's claim shape.
enum class TokenKind {
  kValid,
  kExpired,
  kAudAsString,  // aud="vizier" -- the form both Pixie minting paths emit
  kMissingAud,
  kWrongAud,
  kMissingExp,
  kAlgNone,
  kWrongIss,
  kMissingIss,
  kWrongScope,  // Scopes="user"
  kMissingScope,
  kExtraScopes,  // Scopes="user,service" -- service present among others
};

// MakeToken mints a JWT whose claim shape mirrors GenerateServiceToken.
std::string MakeToken(const std::string& signing_key, TokenKind kind) {
  using std::chrono::seconds;
  using std::chrono::system_clock;
  const auto now = system_clock::now();
  // Well outside the verifier's default 30s leeway in both directions.
  const auto exp_offset = (kind == TokenKind::kExpired) ? seconds{-600} : seconds{600};

  if (kind == TokenKind::kAlgNone) {
    // cpp-jwt won't emit alg:"none" (correctly -- RFC 8725 bans it), so
    // hand-craft the canonical forgery: valid-looking claims, empty signature.
    // Header:  {"alg":"none","typ":"JWT"}
    constexpr char kHeader[] = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0";
    // Payload: {"aud":["vizier"],"exp":4102444800}
    constexpr char kPayload[] = "eyJhdWQiOlsidml6aWVyIl0sImV4cCI6NDEwMjQ0NDgwMH0";
    return std::string(kHeader) + "." + kPayload + ".";
  }

  ::jwt::jwt_object obj{::jwt::params::algorithm("HS256")};
  switch (kind) {
    case TokenKind::kWrongIss:
      obj.add_claim("iss", "not-PL");
      break;
    case TokenKind::kMissingIss:
      break;
    default:
      obj.add_claim("iss", "PL");
  }
  switch (kind) {
    case TokenKind::kAudAsString:
      obj.add_claim("aud", std::string("vizier"));
      break;
    case TokenKind::kWrongAud:
      obj.add_claim("aud", std::vector<std::string>{"wrong-service"});
      break;
    case TokenKind::kMissingAud:
      break;
    default:
      // Array form. Neither Pixie minting path emits this today, but RFC 7519
      // 4.1.3 allows it, so the verifier accepts it.
      obj.add_claim("aud", std::vector<std::string>{"vizier"});
  }
  obj.add_claim("jti", "service-token-test");
  obj.add_claim("iat", now);
  obj.add_claim("nbf", now - std::chrono::seconds{600});
  if (kind != TokenKind::kMissingExp) {
    obj.add_claim("exp", now + exp_offset);
  }
  // sub is the serviceID, not the literal "service" -- the verifier must not
  // assert on it.
  obj.add_claim("sub", "dx");
  switch (kind) {
    case TokenKind::kWrongScope:
      obj.add_claim("Scopes", "user");
      break;
    case TokenKind::kExtraScopes:
      obj.add_claim("Scopes", "user,service");
      break;
    case TokenKind::kMissingScope:
      break;
    default:
      obj.add_claim("Scopes", "service");
  }
  obj.add_claim("ServiceID", "dx-test");
  obj.secret(signing_key);
  return obj.signature();
}

// FlipByteInSegment returns `token` with one character of the given
// dot-separated segment changed, so the segment stays base64url-shaped but the
// signature no longer covers it.
std::string FlipByteInSegment(const std::string& token, int segment) {
  std::vector<size_t> dots;
  for (size_t i = 0; i < token.size(); ++i) {
    if (token[i] == '.') dots.push_back(i);
  }
  EXPECT_EQ(2U, dots.size()) << "expected a three-segment JWT";
  const size_t begin = (segment == 0) ? 0 : dots[segment - 1] + 1;
  const size_t end = (segment == 2) ? token.size() : dots[segment];
  EXPECT_LT(begin, end) << "segment " << segment << " is empty";
  std::string out = token;
  // Base64url alphabet either way, so the segment still decodes.
  out[end - 1] = (out[end - 1] == 'A') ? 'B' : 'A';
  return out;
}

TEST(VerifyServiceJWT, ValidTokenAccepted) {
  EXPECT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), kTestSigningKey));
}

TEST(VerifyServiceJWT, AudAsStringAccepted) {
  EXPECT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kAudAsString), kTestSigningKey));
}

TEST(VerifyServiceJWT, ExtraScopesAccepted) {
  EXPECT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kExtraScopes), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongKeyRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), kWrongSigningKey));
}

TEST(VerifyServiceJWT, ExpiredRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kExpired), kTestSigningKey));
}

TEST(VerifyServiceJWT, MissingExpRejected) {
  // cpp-jwt only validates exp when the claim is present, so a token with no
  // expiry would otherwise verify forever.
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

TEST(VerifyServiceJWT, MissingAudRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kMissingAud), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongIssRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kWrongIss), kTestSigningKey));
}

TEST(VerifyServiceJWT, MissingIssRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kMissingIss), kTestSigningKey));
}

TEST(VerifyServiceJWT, WrongScopeRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kWrongScope), kTestSigningKey));
}

TEST(VerifyServiceJWT, MissingScopeRejected) {
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kMissingScope), kTestSigningKey));
}

TEST(VerifyServiceJWT, TamperedHeaderRejected) {
  const auto tok = MakeToken(kTestSigningKey, TokenKind::kValid);
  EXPECT_NOT_OK(VerifyServiceJWT(FlipByteInSegment(tok, 0), kTestSigningKey));
}

TEST(VerifyServiceJWT, TamperedPayloadRejected) {
  const auto tok = MakeToken(kTestSigningKey, TokenKind::kValid);
  EXPECT_NOT_OK(VerifyServiceJWT(FlipByteInSegment(tok, 1), kTestSigningKey));
}

TEST(VerifyServiceJWT, TamperedSignatureRejected) {
  const auto tok = MakeToken(kTestSigningKey, TokenKind::kValid);
  EXPECT_NOT_OK(VerifyServiceJWT(FlipByteInSegment(tok, 2), kTestSigningKey));
}

TEST(VerifyServiceJWT, TruncatedRejected) {
  auto tok = MakeToken(kTestSigningKey, TokenKind::kValid);
  tok.resize(tok.size() - 10);
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kTestSigningKey));
}

TEST(VerifyServiceJWT, ConcatenatedTokensRejected) {
  const auto tok = MakeToken(kTestSigningKey, TokenKind::kValid);
  EXPECT_NOT_OK(VerifyServiceJWT(tok + "." + tok, kTestSigningKey));
}

TEST(VerifyServiceJWT, GarbageRejected) {
  EXPECT_NOT_OK(VerifyServiceJWT("not-a-jwt-at-all", kTestSigningKey));
}

TEST(VerifyServiceJWT, EmptyTokenRejected) { EXPECT_NOT_OK(VerifyServiceJWT("", kTestSigningKey)); }

TEST(VerifyServiceJWT, EmptySigningKeyRejected) {
  // A misconfigured verifier must fail closed, not accept everything.
  EXPECT_NOT_OK(VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), ""));
}

// HS384-signed token carrying an HS256 header: the algorithm allowlist must
// reject it rather than trusting the header's claim about itself.
TEST(VerifyServiceJWT, AlgConfusionRejected) {
  ::jwt::jwt_object obj{::jwt::params::algorithm("HS384")};
  obj.add_claim("iss", "PL");
  obj.add_claim("aud", std::string("vizier"));
  obj.add_claim("exp", std::chrono::system_clock::now() + std::chrono::seconds{600});
  obj.add_claim("Scopes", "service");
  obj.secret(kTestSigningKey);
  EXPECT_NOT_OK(VerifyServiceJWT(obj.signature(), kTestSigningKey));
}

TEST(VerifyServiceJWT, LeewayAllowsSmallClockSkew) {
  using std::chrono::seconds;
  ::jwt::jwt_object obj{::jwt::params::algorithm("HS256")};
  obj.add_claim("iss", "PL");
  obj.add_claim("aud", std::string("vizier"));
  obj.add_claim("Scopes", "service");
  // Expired 5s ago: rejected with no leeway, accepted with 30s of it.
  obj.add_claim("exp", std::chrono::system_clock::now() - seconds{5});
  obj.secret(kTestSigningKey);
  const auto tok = obj.signature();

  ServiceJWTOptions no_leeway;
  no_leeway.leeway = seconds{0};
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kTestSigningKey, no_leeway));
  EXPECT_OK(VerifyServiceJWT(tok, kTestSigningKey, ServiceJWTOptions{}));
}

TEST(VerifyServiceJWT, CustomOptionsRespected) {
  ServiceJWTOptions opts;
  opts.audience = "some-other-audience";
  EXPECT_NOT_OK(
      VerifyServiceJWT(MakeToken(kTestSigningKey, TokenKind::kValid), kTestSigningKey, opts));
}

TEST(GenerateServiceToken, RoundTripsThroughVerify) {
  ASSERT_OK_AND_ASSIGN(const auto tok, GenerateServiceToken(kTestSigningKey, "pem"));
  EXPECT_OK(VerifyServiceJWT(tok, kTestSigningKey));
}

TEST(GenerateServiceToken, RejectsWrongKeyOnVerify) {
  ASSERT_OK_AND_ASSIGN(const auto tok, GenerateServiceToken(kTestSigningKey, "pem"));
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kWrongSigningKey));
}

TEST(GenerateServiceToken, EmptyKeyReturnsErrorInsteadOfThrowing) {
  // cpp-jwt raises jwt::SigningError on an empty secret; letting that escape
  // would abort the process on the first outgoing service call.
  EXPECT_NOT_OK(GenerateServiceToken("", "pem"));
}

TEST(GenerateServiceToken, RecordsServiceID) {
  ASSERT_OK_AND_ASSIGN(const auto tok, GenerateServiceToken(kTestSigningKey, "my-service"));
  std::error_code ec;
  const auto obj = ::jwt::decode(tok, ::jwt::params::algorithms({"HS256"}), ec,
                                 ::jwt::params::secret(std::string(kTestSigningKey)),
                                 ::jwt::params::verify(true));
  ASSERT_FALSE(ec) << ec.message();
  EXPECT_EQ("my-service", obj.payload().get_claim_value<std::string>("ServiceID"));
  EXPECT_EQ("service", obj.payload().get_claim_value<std::string>("Scopes"));
}

TEST(GenerateServiceToken, ShortValidityExpires) {
  // valid_for is honored: a token minted already-expired must not verify.
  ASSERT_OK_AND_ASSIGN(const auto tok,
                       GenerateServiceToken(kTestSigningKey, "pem", std::chrono::seconds{-600}));
  EXPECT_NOT_OK(VerifyServiceJWT(tok, kTestSigningKey));
}

TEST(StripBearerPrefix, AcceptsBothCasings) {
  EXPECT_EQ("tok", StripBearerPrefix("bearer tok"));
  EXPECT_EQ("tok", StripBearerPrefix("Bearer tok"));
  EXPECT_EQ("tok", StripBearerPrefix("BEARER tok"));
}

TEST(StripBearerPrefix, RejectsOtherSchemesAndMalformedValues) {
  EXPECT_EQ("", StripBearerPrefix("Token tok"));
  EXPECT_EQ("", StripBearerPrefix("tok"));
  EXPECT_EQ("", StripBearerPrefix("Bearer"));
  EXPECT_EQ("", StripBearerPrefix(""));
  // Prefix present but nothing after it.
  EXPECT_EQ("", StripBearerPrefix("bearer "));
}

}  // namespace services
}  // namespace px
