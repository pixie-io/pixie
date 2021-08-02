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

package authcontext_test

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/jwtpb"
	"px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestSessionCtx_UseJWTAuth(t *testing.T) {
	token := testingutils.GenerateTestJWTToken(t, "signing_key")

	ctx := authcontext.New()
	err := ctx.UseJWTAuth("signing_key", token, "withpixie.ai")
	require.NoError(t, err)

	assert.Equal(t, testingutils.TestUserID, ctx.Claims.Subject)
	assert.Equal(t, "test@test.com", ctx.Claims.GetUserClaims().Email)
}

func TestSessionCtx_ValidClaims(t *testing.T) {
	tests := []struct {
		name          string
		expiryFromNow time.Duration
		claims        *jwtpb.JWTClaims
		isValid       bool
	}{
		{
			name:    "no claims",
			isValid: false,
		},
		{
			name:          "valid user claims",
			isValid:       true,
			claims:        testingutils.GenerateTestClaimsWithDuration(t, time.Minute*60, "test@test.com"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "expired user claims",
			isValid:       false,
			claims:        testingutils.GenerateTestClaimsWithDuration(t, time.Minute*60, "test@test.com"),
			expiryFromNow: -1 * time.Second,
		},
		{
			name:          "api user claims",
			isValid:       true,
			claims:        utils.GenerateJWTForAPIUser("6ba7b810-9dad-11d1-80b4-00c04fd430c9", "6ba7b810-9dad-11d1-80b4-00c04fd430c8", time.Now().Add(time.Minute*60), "withpixie.ai"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "valid service claims",
			isValid:       true,
			claims:        testingutils.GenerateTestServiceClaims(t, "vzmgr"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "expired service claims",
			isValid:       false,
			claims:        testingutils.GenerateTestServiceClaims(t, "vzmgr"),
			expiryFromNow: -1 * time.Second,
		},
		{
			name:          "valid cluster claims",
			isValid:       true,
			claims:        utils.GenerateJWTForCluster("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "withpixie.ai"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "invalid cluster ID",
			isValid:       false,
			claims:        utils.GenerateJWTForCluster("some text", "withpixie.ai"),
			expiryFromNow: time.Minute * 60,
		},
		{
			name:          "expired cluster claims",
			isValid:       false,
			claims:        utils.GenerateJWTForCluster("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "withpixie.ai"),
			expiryFromNow: -1 * time.Second,
		},
		{
			name:    "claims with no type",
			isValid: false,
			claims: &jwtpb.JWTClaims{
				Subject:   "test subject",
				Audience:  "withpixie.ai",
				IssuedAt:  time.Now().Unix(),
				ExpiresAt: time.Now().Add(time.Minute * 10).Unix(),
			},
			expiryFromNow: time.Minute * 60,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			ctx := authcontext.New()
			if tc.claims != nil {
				token := testingutils.SignPBClaims(t, tc.claims, "signing_key")
				err := ctx.UseJWTAuth("signing_key", token, "withpixie.ai")
				require.NoError(t, err)

				ctx.Claims.ExpiresAt = time.Now().Add(tc.expiryFromNow).Unix()
			}

			assert.Equal(t, tc.isValid, ctx.ValidClaims())
		})
	}
}
