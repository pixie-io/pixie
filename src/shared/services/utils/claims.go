package utils

import (
	"errors"
	"strings"
	"time"

	"github.com/dgrijalva/jwt-go/v4"
	log "github.com/sirupsen/logrus"

	jwt2 "pixielabs.ai/pixielabs/src/shared/services/proto"
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
func PBToMapClaims(pb *jwt2.JWTClaims) jwt.MapClaims {
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
	case *jwt2.JWTClaims_UserClaims:
		claims["UserID"] = m.UserClaims.UserID
		claims["OrgID"] = m.UserClaims.OrgID
		claims["Email"] = m.UserClaims.Email
	case *jwt2.JWTClaims_ServiceClaims:
		claims["ServiceID"] = m.ServiceClaims.ServiceID
	case *jwt2.JWTClaims_ClusterClaims:
		claims["ClusterID"] = m.ClusterClaims.ClusterID
	default:
		log.WithField("type", m).Error("Could not find claims type")
	}

	return claims
}

// GetClaimsType gets the type of the given claim.
func GetClaimsType(c *jwt2.JWTClaims) ClaimType {
	switch c.CustomClaims.(type) {
	case *jwt2.JWTClaims_UserClaims:
		return UserClaimType
	case *jwt2.JWTClaims_ServiceClaims:
		return ServiceClaimType
	case *jwt2.JWTClaims_ClusterClaims:
		return ClusterClaimType
	default:
		return UnknownClaimType
	}
}

// MapClaimsToPB tkes a MapClaims and converts it to a protobuf.
func MapClaimsToPB(claims jwt.MapClaims) (*jwt2.JWTClaims, error) {
	p := &jwt2.JWTClaims{}
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
		userClaims := &jwt2.UserJWTClaims{
			UserID: claims["UserID"].(string),
			OrgID:  claims["OrgID"].(string),
			Email:  claims["Email"].(string),
		}
		p.CustomClaims = &jwt2.JWTClaims_UserClaims{
			UserClaims: userClaims,
		}
	case claims["ServiceID"] != nil:
		serviceClaims := &jwt2.ServiceJWTClaims{
			ServiceID: claims["ServiceID"].(string),
		}
		p.CustomClaims = &jwt2.JWTClaims_ServiceClaims{
			ServiceClaims: serviceClaims,
		}
	case claims["ClusterID"] != nil:
		clusterClaims := &jwt2.ClusterJWTClaims{
			ClusterID: claims["ClusterID"].(string),
		}
		p.CustomClaims = &jwt2.JWTClaims_ClusterClaims{
			ClusterClaims: clusterClaims,
		}
	}

	return p, nil
}

// GenerateJWTForUser creates a protobuf claims for the given user.
func GenerateJWTForUser(userID string, orgID string, email string, expiresAt time.Time, audience string) *jwt2.JWTClaims {
	claims := jwt2.JWTClaims{
		Subject: userID,
		// Standard claims.
		Audience:  audience,
		ExpiresAt: expiresAt.Unix(),
		IssuedAt:  time.Now().Unix(),
		Issuer:    "PL",
		Scopes:    []string{"user"},
	}
	claims.CustomClaims = &jwt2.JWTClaims_UserClaims{
		UserClaims: &jwt2.UserJWTClaims{
			Email:  email,
			UserID: userID,
			OrgID:  orgID,
		},
	}
	return &claims
}

// GenerateJWTForService creates a protobuf claims for the given service.
func GenerateJWTForService(serviceID string, audience string) *jwt2.JWTClaims {
	pbClaims := jwt2.JWTClaims{
		Audience:  audience,
		Subject:   serviceID,
		Issuer:    "PL",
		ExpiresAt: time.Now().Add(time.Minute * 10).Unix(),
		Scopes:    []string{"service"},
		CustomClaims: &jwt2.JWTClaims_ServiceClaims{
			ServiceClaims: &jwt2.ServiceJWTClaims{
				ServiceID: serviceID,
			},
		},
	}
	return &pbClaims
}

// GenerateJWTForCluster creates a protobuf claims for the given cluster.
func GenerateJWTForCluster(clusterID string, audience string) *jwt2.JWTClaims {
	pbClaims := jwt2.JWTClaims{
		Audience:  audience,
		ExpiresAt: time.Now().Add(time.Hour).Unix(),
		// The IssuedAt begins earlier, to give leeway for user's clusters
		// which may have some clock skew.
		IssuedAt:  time.Now().Add(-2 * time.Minute).Unix(),
		NotBefore: time.Now().Add(-2 * time.Minute).Unix(),
		Issuer:    "pixielabs.ai",
		Subject:   "pixielabs.ai/vizier",
		Scopes:    []string{"cluster"},
		CustomClaims: &jwt2.JWTClaims_ClusterClaims{
			ClusterClaims: &jwt2.ClusterJWTClaims{
				ClusterID: clusterID,
			},
		},
	}
	return &pbClaims
}

// SignJWTClaims signs the claim using the given signing key.
func SignJWTClaims(claims *jwt2.JWTClaims, signingKey string) (string, error) {
	mc := PBToMapClaims(claims)
	return jwt.NewWithClaims(jwt.SigningMethodHS256, mc).SignedString([]byte(signingKey))
}
