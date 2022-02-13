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

package testingutils

import (
	"testing"
	"time"

	"px.dev/pixie/src/shared/services/jwtpb"
	"px.dev/pixie/src/shared/services/utils"
)

// TestOrgID is a test org valid UUID
const TestOrgID string = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"

// TestUserID is a test user valid UUID
const TestUserID string = "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

// GenerateTestClaimsWithDuration generates valid test user claims for a specified duration.
func GenerateTestClaimsWithDuration(t *testing.T, duration time.Duration, email string) *jwtpb.JWTClaims {
	claims := utils.GenerateJWTForUser(TestUserID, TestOrgID, email, time.Now().Add(duration), "withpixie.ai")
	return claims
}

// GenerateTestServiceClaims generates valid test service claims for a specified duration.
func GenerateTestServiceClaims(t *testing.T, service string) *jwtpb.JWTClaims {
	claims := utils.GenerateJWTForService(service, "withpixie.ai")
	return claims
}

// GenerateTestClaims generates valid test user claims valid for 60 minutes
func GenerateTestClaims(t *testing.T) *jwtpb.JWTClaims {
	return GenerateTestClaimsWithDuration(t, time.Minute*60, "test@test.com")
}

// GenerateTestClaimsWithEmail generates valid test user claims for the given email.
func GenerateTestClaimsWithEmail(t *testing.T, email string) *jwtpb.JWTClaims {
	return GenerateTestClaimsWithDuration(t, time.Minute*60, email)
}

// GenerateTestJWTToken generates valid tokens for testing.
func GenerateTestJWTToken(t *testing.T, signingKey string) string {
	return GenerateTestJWTTokenWithDuration(t, signingKey, time.Minute*60)
}

// GenerateTestJWTTokenWithDuration generates valid tokens for testing with the specified duration.
func GenerateTestJWTTokenWithDuration(t *testing.T, signingKey string, timeout time.Duration) string {
	claims := GenerateTestClaimsWithDuration(t, timeout, "test@test.com")

	return SignPBClaims(t, claims, signingKey)
}

// SignPBClaims signs our protobuf claims after converting to json.
func SignPBClaims(t *testing.T, claims *jwtpb.JWTClaims, signingKey string) string {
	signed, err := utils.SignJWTClaims(claims, signingKey)
	if err != nil {
		t.Fatal("failed to generate token")
	}
	return signed
}
