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
	"errors"
	"strings"
	"time"

	"github.com/dgrijalva/jwt-go/v4"
	log "github.com/sirupsen/logrus"

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

// PBToMapClaims maps protobuf claims to map claims.
func PBToMapClaims(pb *jwtpb.JWTClaims) jwt.MapClaims {
	claims := jwt.MapClaims{}

	// Standard claims.
	claims["aud"] = pb.Audience
	claims["exp"] = pb.ExpiresAt
	claims["jti"] = pb.JTI
	claims["iat"] = pb.IssuedAt
	claims["iss"] = pb.Issuer
	claims["nbf"] = pb.NotBefore
	claims["sub"] = pb.Subject

	// Custom claims.
	claims["Scopes"] = strings.Join(pb.Scopes, ",")

	switch m := pb.CustomClaims.(type) {
	case *jwtpb.JWTClaims_UserClaims:
		claims["UserID"] = m.UserClaims.UserID
		claims["OrgID"] = m.UserClaims.OrgID
		claims["Email"] = m.UserClaims.Email
	case *jwtpb.JWTClaims_ServiceClaims:
		claims["ServiceID"] = m.ServiceClaims.ServiceID
	case *jwtpb.JWTClaims_ClusterClaims:
		claims["ClusterID"] = m.ClusterClaims.ClusterID
	default:
		log.WithField("type", m).Error("Could not find claims type")
	}

	return claims
}

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

// MapClaimsToPB tkes a MapClaims and converts it to a protobuf.
func MapClaimsToPB(claims jwt.MapClaims) (*jwtpb.JWTClaims, error) {
	p := &jwtpb.JWTClaims{}
	var ok bool

	// Standard claims.
	p.Audience, ok = claims["aud"].(string)
	if !ok {
		return nil, errors.New("JWT claim audience is not a string")
	}
	expAt, ok := claims["exp"].(float64)
	if !ok {
		return nil, errors.New("JWT claim expiresAt is not a float")
	}
	p.ExpiresAt = int64(expAt)

	p.JTI, ok = claims["jti"].(string)
	if !ok {
		return nil, errors.New("JWT claim JTI is not a string")
	}
	isAt, ok := claims["iat"].(float64)
	if !ok {
		return nil, errors.New("JWT claim IssuedAt is not a float")
	}
	p.IssuedAt = int64(isAt)

	p.Issuer, ok = claims["iss"].(string)
	if !ok {
		return nil, errors.New("JWT claim Issuer is not a string")
	}
	nbf, ok := claims["nbf"].(float64)
	if !ok {
		return nil, errors.New("JWT claim notBefore is not a float")
	}
	p.NotBefore = int64(nbf)

	p.Subject, ok = claims["sub"].(string)
	if !ok {
		return nil, errors.New("JWT claim subject is not a string")
	}
	scopes, ok := claims["Scopes"].(string)
	if !ok {
		return nil, errors.New("JWT claim scopes is not a string")
	}

	p.Scopes = strings.Split(scopes, ",")

	// Custom claims.
	switch {
	case claims["UserID"] != nil:
		userClaims := &jwtpb.UserJWTClaims{
			UserID: claims["UserID"].(string),
			OrgID:  claims["OrgID"].(string),
			Email:  claims["Email"].(string),
		}
		p.CustomClaims = &jwtpb.JWTClaims_UserClaims{
			UserClaims: userClaims,
		}
	case claims["ServiceID"] != nil:
		serviceClaims := &jwtpb.ServiceJWTClaims{
			ServiceID: claims["ServiceID"].(string),
		}
		p.CustomClaims = &jwtpb.JWTClaims_ServiceClaims{
			ServiceClaims: serviceClaims,
		}
	case claims["ClusterID"] != nil:
		clusterClaims := &jwtpb.ClusterJWTClaims{
			ClusterID: claims["ClusterID"].(string),
		}
		p.CustomClaims = &jwtpb.JWTClaims_ClusterClaims{
			ClusterClaims: clusterClaims,
		}
	}

	return p, nil
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

// SignJWTClaims signs the claim using the given signing key.
func SignJWTClaims(claims *jwtpb.JWTClaims, signingKey string) (string, error) {
	mc := PBToMapClaims(claims)
	return jwt.NewWithClaims(jwt.SigningMethodHS256, mc).SignedString([]byte(signingKey))
}
