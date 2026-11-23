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

#include "src/stirling/source_connectors/socket_tracer/protocols/http/credentials.h"

#include <map>
#include <string>

#include <absl/strings/escaping.h>
#include <absl/strings/str_cat.h>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http {

std::string Redacted(std::string_view secret) {
  return absl::StrCat("<cred:sha256=", CredentialFingerprint(secret), ">");
}

std::string Jwt(std::string_view header, std::string_view payload) {
  return absl::StrCat(absl::WebSafeBase64Escape(header), ".", absl::WebSafeBase64Escape(payload),
                      ".c2lnbmF0dXJl");
}

TEST(CredentialsTest, FingerprintIsSha256Prefix) {
  EXPECT_EQ(CredentialFingerprint("abc"), "ba7816bf8f01cfea");
}

TEST(CredentialsTest, BearerJwtKeepsClaimsAndDropsToken) {
  const std::string token = Jwt(
      R"({"alg":"RS256","kid":"k1"})",
      R"({"iss":"https://kubernetes.default.svc","sub":"system:serviceaccount:credscope:sa-over",)"
      R"("aud":["https://kubernetes.default.svc"],"exp":1763600000,"iat":1763599400,)"
      R"("kubernetes.io":{"namespace":"credscope"},"canary":"cs-canary-1"})");

  std::optional<std::string> redacted =
      RedactCredentialHeader("Authorization", absl::StrCat("Bearer ", token));

  ASSERT_TRUE(redacted.has_value());
  EXPECT_EQ(redacted.value(),
            absl::StrCat("Bearer <cred:sha256=", CredentialFingerprint(token),
                         R"( jwt={"alg":"RS256","iss":"https://kubernetes.default.svc",)"
                         R"("sub":"system:serviceaccount:credscope:sa-over",)"
                         R"("aud":["https://kubernetes.default.svc"],"exp":1763600000,)"
                         R"("iat":1763599400,"kubernetes.io":{"namespace":"credscope"}}>)"));
}

TEST(CredentialsTest, OauthScopesAreKept) {
  const std::string token =
      Jwt(R"({"alg":"ES256"})",
          R"({"iss":"https://idp","sub":"orders-client","azp":"orders","client_id":"orders",)"
          R"("scope":"orders:read admin","scp":["orders:read"],"nbf":1,"exp":2})");

  EXPECT_EQ(RedactCredentialHeader("authorization", absl::StrCat("bearer ", token)),
            absl::StrCat("bearer <cred:sha256=", CredentialFingerprint(token),
                         R"( jwt={"alg":"ES256","iss":"https://idp","sub":"orders-client",)"
                         R"("azp":"orders","client_id":"orders","scope":"orders:read admin",)"
                         R"("scp":["orders:read"],"exp":2,"nbf":1}>)"));
}

TEST(CredentialsTest, OpaqueBearerIsFingerprinted) {
  EXPECT_EQ(RedactCredentialHeader("Authorization", "Bearer sk-canary-123"),
            absl::StrCat("Bearer ", Redacted("sk-canary-123")));
}

TEST(CredentialsTest, MalformedJwtIsFingerprinted) {
  EXPECT_EQ(RedactCredentialHeader("Authorization", "Bearer a.b.c"),
            absl::StrCat("Bearer ", Redacted("a.b.c")));
}

TEST(CredentialsTest, BasicKeepsUser) {
  const std::string encoded = absl::Base64Escape("alice:cs-canary-2");
  EXPECT_EQ(RedactCredentialHeader("Proxy-Authorization", absl::StrCat("Basic ", encoded)),
            absl::StrCat("Basic <cred:sha256=", CredentialFingerprint(encoded), " user=alice>"));
}

TEST(CredentialsTest, SchemelessValueIsFingerprinted) {
  EXPECT_EQ(RedactCredentialHeader("Authorization", "cs-canary-3"), Redacted("cs-canary-3"));
}

TEST(CredentialsTest, ApiKeyHeadersAreFingerprinted) {
  EXPECT_EQ(RedactCredentialHeader("X-Api-Key", "cs-canary-4"), Redacted("cs-canary-4"));
  EXPECT_EQ(RedactCredentialHeader("x-auth-token", "cs-canary-5"), Redacted("cs-canary-5"));
}

TEST(CredentialsTest, CookieValuesAreFingerprinted) {
  EXPECT_EQ(RedactCredentialHeader("Cookie", "theme=dark; session=cs-canary-6"),
            absl::StrCat("theme=", Redacted("dark"), "; session=", Redacted("cs-canary-6")));
}

TEST(CredentialsTest, SetCookieKeepsAttributes) {
  EXPECT_EQ(RedactCredentialHeader("Set-Cookie", "session=cs-canary-7; Path=/; HttpOnly"),
            absl::StrCat("session=", Redacted("cs-canary-7"), "; Path=/; HttpOnly"));
}

TEST(CredentialsTest, WebSocketBearerSubprotocolIsRedacted) {
  const std::string token = Jwt(R"({"alg":"RS256"})", R"({"sub":"system:serviceaccount:a:b"})");
  const std::string encoded = absl::WebSafeBase64Escape(token);
  EXPECT_EQ(RedactCredentialHeader(
                "Sec-WebSocket-Protocol",
                absl::StrCat("v5.channel.k8s.io, base64url.bearer.authorization.k8s.io.", encoded)),
            absl::StrCat("v5.channel.k8s.io, base64url.bearer.authorization.k8s.io.<cred:sha256=",
                         CredentialFingerprint(encoded),
                         R"( jwt={"alg":"RS256","sub":"system:serviceaccount:a:b"}>)"));
  EXPECT_EQ(RedactCredentialHeader("sec-websocket-protocol", "v4.channel.k8s.io"),
            "v4.channel.k8s.io");
}

TEST(CredentialsTest, OtherHeadersAreKept) {
  EXPECT_EQ(RedactCredentialHeader("Content-Type", "application/json"), std::nullopt);
  EXPECT_EQ(RedactCredentialHeader("Audit-Id", "6f1c"), std::nullopt);
}

TEST(CredentialsTest, QueryCredentialsAreFingerprinted) {
  EXPECT_EQ(RedactCredentialPath("/ws?room=1&access_token=cs-canary-8&API_KEY=cs-canary-9#frag"),
            absl::StrCat("/ws?room=1&access_token=", Redacted("cs-canary-8"),
                         "&API_KEY=", Redacted("cs-canary-9"), "#frag"));
  EXPECT_EQ(RedactCredentialPath("/api/v1/pods?watch=1"), "/api/v1/pods?watch=1");
}

TEST(CredentialsTest, UrlHeadersAreRedacted) {
  EXPECT_EQ(RedactCredentialHeader("Referer", "https://app/cb?code=1&id_token=cs-canary-10"),
            absl::StrCat("https://app/cb?code=1&id_token=", Redacted("cs-canary-10")));
}

TEST(CredentialsTest, JsonFieldsAreFingerprinted) {
  EXPECT_EQ(RedactCredentialBody(
                R"({"token_type":"Bearer","access_token":"cs-canary-11","expires_in":600,)"
                R"("Password" : "a\"b"})"),
            absl::StrCat(R"({"token_type":"Bearer","access_token":")", Redacted("cs-canary-11"),
                         R"(","expires_in":600,"Password" : ")", Redacted(R"(a\"b)"), R"("})"));
}

TEST(CredentialsTest, TruncatedJsonFieldIsFingerprinted) {
  EXPECT_EQ(RedactCredentialBody(R"({"refresh_token":"cs-canary-1)"),
            absl::StrCat(R"({"refresh_token":")", Redacted("cs-canary-1")));
}

TEST(CredentialsTest, TextProtoFieldsAreFingerprinted) {
  EXPECT_EQ(RedactCredentialBody(R"(client_secret: "cs-canary-12" name: "x")"),
            absl::StrCat(R"(client_secret: ")", Redacted("cs-canary-12"), R"(" name: "x")"));
}

TEST(CredentialsTest, FormFieldsAreFingerprinted) {
  EXPECT_EQ(RedactCredentialBody("grant_type=client_credentials&client_secret=cs-canary-13"),
            absl::StrCat("grant_type=client_credentials&client_secret=", Redacted("cs-canary-13")));
}

TEST(CredentialsTest, NonCredentialFieldsAreKept) {
  EXPECT_EQ(RedactCredentialBody(R"({"tokens":3,"user":"bob","my_password_hint":"x"})"),
            R"({"tokens":3,"user":"bob","my_password_hint":"x"})");
}

TEST(CredentialsTest, HeaderMapsAreRedacted) {
  HeadersMap headers = {{"Authorization", "Bearer cs-canary-14"}, {"Accept", "*/*"}};
  RedactCredentials(&headers);
  EXPECT_EQ(headers.find("authorization")->second,
            absl::StrCat("Bearer ", Redacted("cs-canary-14")));
  EXPECT_EQ(headers.find("accept")->second, "*/*");

  std::multimap<std::string, std::string> nv = {{":path", "/svc.Orders/Get?token=cs-canary-15"},
                                                {"authorization", "Bearer cs-canary-16"}};
  RedactCredentials(&nv);
  EXPECT_EQ(nv.find(":path")->second,
            absl::StrCat("/svc.Orders/Get?token=", Redacted("cs-canary-15")));
  EXPECT_EQ(nv.find("authorization")->second, absl::StrCat("Bearer ", Redacted("cs-canary-16")));
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
