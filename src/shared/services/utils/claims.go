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

package utils

import (
	"time"

	"px.dev/pixie/src/shared/services/jwtpb"
)

// ClaimType represents the type of claims we allow in our system.
type ClaimType int

const (
	// UnknownClaimType is an unknown type.
	UnknownClaimType ClaimType = iota
	// UserClaimType is a claim for a user.
	UserClaimType
	// ServiceClaimType is a claim for a service.
	ServiceClaimType
	// ClusterClaimType is a claim type for a cluster.
	ClusterClaimType
)

// GetClaimsType gets the type of the given claim.
func GetClaimsType(c *jwtpb.JWTClaims) ClaimType {
	switch c.CustomClaims.(type) {
	case *jwtpb.JWTClaims_UserClaims:
		return UserClaimType
	case *jwtpb.JWTClaims_ServiceClaims:
		return ServiceClaimType
	case *jwtpb.JWTClaims_ClusterClaims:
		return ClusterClaimType
	default:
		return UnknownClaimType
	}
}

// GenerateJWTForUser creates a protobuf claims for the given user.
func GenerateJWTForUser(userID string, orgID string, email string, expiresAt time.Time, audience string) *jwtpb.JWTClaims {
	claims := jwtpb.JWTClaims{
		Subject: userID,
		// Standard claims.
		Audience:  audience,
		ExpiresAt: expiresAt.Unix(),
		IssuedAt:  time.Now().Unix(),
		Issuer:    "PL",
		Scopes:    []string{"user"},
	}
	claims.CustomClaims = &jwtpb.JWTClaims_UserClaims{
		UserClaims: &jwtpb.UserJWTClaims{
			Email:  email,
			UserID: userID,
			OrgID:  orgID,
		},
	}
	return &claims
}

// GenerateJWTForAPIUser creates a protobuf claims for the api user.
func GenerateJWTForAPIUser(userID string, orgID string, expiresAt time.Time, audience string) *jwtpb.JWTClaims {
	claims := jwtpb.JWTClaims{
		Subject: orgID,
		// Standard claims.
		Audience:  audience,
		ExpiresAt: expiresAt.Unix(),
		IssuedAt:  time.Now().Unix(),
		Issuer:    "PL",
		Scopes:    []string{"user"},
	}
	claims.CustomClaims = &jwtpb.JWTClaims_UserClaims{
		UserClaims: &jwtpb.UserJWTClaims{
			OrgID:     orgID,
			UserID:    userID,
			IsAPIUser: true,
		},
	}
	return &claims
}

// GenerateJWTForService creates a protobuf claims for the given service.
func GenerateJWTForService(serviceID string, audience string) *jwtpb.JWTClaims {
	pbClaims := jwtpb.JWTClaims{
		Audience:  audience,
		Subject:   serviceID,
		Issuer:    "PL",
		ExpiresAt: time.Now().Add(time.Minute * 10).Unix(),
		Scopes:    []string{"service"},
		CustomClaims: &jwtpb.JWTClaims_ServiceClaims{
			ServiceClaims: &jwtpb.ServiceJWTClaims{
				ServiceID: serviceID,
			},
		},
	}
	return &pbClaims
}

// GenerateJWTForCluster creates a protobuf claims for the given cluster.
func GenerateJWTForCluster(clusterID string, audience string) *jwtpb.JWTClaims {
	pbClaims := jwtpb.JWTClaims{
		Audience:  audience,
		ExpiresAt: time.Now().Add(time.Hour).Unix(),
		// The IssuedAt begins earlier, to give leeway for user's clusters
		// which may have some clock skew.
		IssuedAt:  time.Now().Add(-2 * time.Minute).Unix(),
		NotBefore: time.Now().Add(-2 * time.Minute).Unix(),
		Issuer:    "pixielabs.ai",
		Subject:   "pixielabs.ai/vizier",
		Scopes:    []string{"cluster"},
		CustomClaims: &jwtpb.JWTClaims_ClusterClaims{
			ClusterClaims: &jwtpb.ClusterJWTClaims{
				ClusterID: clusterID,
			},
		},
	}
	return &pbClaims
}
