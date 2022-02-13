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
	"time"

	"github.com/lestrrat-go/jwx/jwt"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/jwtpb"
	"px.dev/pixie/src/shared/services/utils"
)

func getStandardClaimsPb() *jwtpb.JWTClaims {
	return &jwtpb.JWTClaims{
		Audience:  "audience",
		ExpiresAt: 100,
		JTI:       "jti",
		IssuedAt:  15,
		Issuer:    "issuer",
		NotBefore: 5,
		Subject:   "subject",
	}
}

func getStandardClaimsBuilder() *jwt.Builder {
	return jwt.NewBuilder().
		Audience([]string{"audience"}).
		Expiration(time.Unix(100, 0)).
		IssuedAt(time.Unix(15, 0)).
		Issuer("issuer").
		JwtID("jti").
		NotBefore(time.Unix(5, 0)).
		Subject("subject")
}

func TestProtoToToken_Standard(t *testing.T) {
	p := getStandardClaimsPb()

	token, err := utils.ProtoToToken(p)
	require.NoError(t, err)
	assert.Equal(t, "audience", token.Audience()[0])
	assert.Equal(t, int64(100), token.Expiration().Unix())
	assert.Equal(t, "jti", token.JwtID())
	assert.Equal(t, int64(15), token.IssuedAt().Unix())
	assert.Equal(t, "issuer", token.Issuer())
	assert.Equal(t, int64(5), token.NotBefore().Unix())
	assert.Equal(t, "subject", token.Subject())
}
func TestProtoToToken_User(t *testing.T) {
	p := getStandardClaimsPb()
	p.Scopes = []string{"user"}
	// User claims.
	userClaims := &jwtpb.UserJWTClaims{
		UserID:    "user_id",
		OrgID:     "org_id",
		Email:     "user@email.com",
		IsAPIUser: false,
	}
	p.CustomClaims = &jwtpb.JWTClaims_UserClaims{
		UserClaims: userClaims,
	}

	token, err := utils.ProtoToToken(p)
	require.NoError(t, err)

	assert.Equal(t, []string{"user"}, utils.GetScopes(token))
	assert.Equal(t, "user_id", utils.GetUserID(token))
	assert.Equal(t, "org_id", utils.GetOrgID(token))
	assert.Equal(t, "user@email.com", utils.GetEmail(token))
	assert.Equal(t, false, utils.GetIsAPIUser(token))
}

func TestProtoToToken_Service(t *testing.T) {
	p := getStandardClaimsPb()
	p.Scopes = []string{"service"}
	// Service claims.
	svcClaims := &jwtpb.ServiceJWTClaims{
		ServiceID: "service_id",
	}
	p.CustomClaims = &jwtpb.JWTClaims_ServiceClaims{
		ServiceClaims: svcClaims,
	}

	token, err := utils.ProtoToToken(p)
	require.NoError(t, err)

	assert.Equal(t, []string{"service"}, utils.GetScopes(token))
	assert.Equal(t, "service_id", utils.GetServiceID(token))
}

func TestProtoToToken_Cluster(t *testing.T) {
	p := getStandardClaimsPb()
	p.Scopes = []string{"cluster"}
	// Cluster claims.
	clusterClaims := &jwtpb.ClusterJWTClaims{
		ClusterID: "cluster_id",
	}
	p.CustomClaims = &jwtpb.JWTClaims_ClusterClaims{
		ClusterClaims: clusterClaims,
	}

	token, err := utils.ProtoToToken(p)
	require.NoError(t, err)

	assert.Equal(t, []string{"cluster"}, utils.GetScopes(token))
	assert.Equal(t, "cluster_id", utils.GetClusterID(token))
}

func TestTokenToProto_Standard(t *testing.T) {
	builder := getStandardClaimsBuilder()

	token, err := builder.Build()
	require.NoError(t, err)

	pb, err := utils.TokenToProto(token)
	require.NoError(t, err)
	assert.Equal(t, "audience", pb.Audience)
	assert.Equal(t, int64(100), pb.ExpiresAt)
	assert.Equal(t, "jti", pb.JTI)
	assert.Equal(t, int64(15), pb.IssuedAt)
	assert.Equal(t, "issuer", pb.Issuer)
	assert.Equal(t, int64(5), pb.NotBefore)
	assert.Equal(t, "subject", pb.Subject)
}

func TestTokenToProto_User(t *testing.T) {
	builder := getStandardClaimsBuilder().
		Claim("Scopes", "user").
		Claim("UserID", "user_id").
		Claim("OrgID", "org_id").
		Claim("Email", "user@email.com").
		Claim("IsAPIUser", false)

	token, err := builder.Build()
	require.NoError(t, err)

	pb, err := utils.TokenToProto(token)
	require.NoError(t, err)
	assert.Equal(t, []string{"user"}, pb.Scopes)
	customClaims := pb.GetUserClaims()
	assert.Equal(t, "user_id", customClaims.UserID)
	assert.Equal(t, "org_id", customClaims.OrgID)
	assert.Equal(t, "user@email.com", customClaims.Email)
	assert.Equal(t, false, customClaims.IsAPIUser)
}

func TestTokenToProto_Service(t *testing.T) {
	builder := getStandardClaimsBuilder().
		Claim("Scopes", "service").
		Claim("ServiceID", "service_id")

	token, err := builder.Build()
	require.NoError(t, err)

	pb, err := utils.TokenToProto(token)
	require.NoError(t, err)
	assert.Equal(t, []string{"service"}, pb.Scopes)
	customClaims := pb.GetServiceClaims()
	assert.Equal(t, "service_id", customClaims.ServiceID)
}

func TestTokenToProto_Cluster(t *testing.T) {
	builder := getStandardClaimsBuilder().
		Claim("Scopes", "cluster").
		Claim("ClusterID", "cluster_id")

	token, err := builder.Build()
	require.NoError(t, err)

	pb, err := utils.TokenToProto(token)
	require.NoError(t, err)
	assert.Equal(t, []string{"cluster"}, pb.Scopes)
	customClaims := pb.GetClusterClaims()
	assert.Equal(t, "cluster_id", customClaims.ClusterID)
}

func TestTokenToProto_FailNoAudience(t *testing.T) {
	builder := jwt.NewBuilder().
		Expiration(time.Unix(100, 0)).
		IssuedAt(time.Unix(15, 0)).
		Issuer("issuer").
		JwtID("jti").
		NotBefore(time.Unix(5, 0)).
		Subject("subject")

	token, err := builder.Build()
	require.NoError(t, err)

	_, err = utils.TokenToProto(token)
	assert.Error(t, err)
}
