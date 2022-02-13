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

package utils_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/shared/services/jwtpb"
	"px.dev/pixie/src/shared/services/utils"
)

func TestGetClaimsType(t *testing.T) {
	p := getStandardClaimsPb()
	p.Scopes = []string{"user"}
	// User claims.
	userClaims := &jwtpb.UserJWTClaims{
		UserID: "user_id",
		OrgID:  "org_id",
		Email:  "user@email.com",
	}
	p.CustomClaims = &jwtpb.JWTClaims_UserClaims{
		UserClaims: userClaims,
	}

	assert.Equal(t, utils.UserClaimType, utils.GetClaimsType(p))
}
