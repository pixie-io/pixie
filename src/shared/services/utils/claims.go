package utils

import (
	"strings"
	"time"

	"github.com/dgrijalva/jwt-go"
	log "github.com/sirupsen/logrus"
	jwt2 "pixielabs.ai/pixielabs/src/shared/services/proto"
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

// MapClaimsToPB tkes a MapClaims and converts it to a protobuf.
func MapClaimsToPB(claims jwt.MapClaims) *jwt2.JWTClaims {
	p := &jwt2.JWTClaims{}

	// Standard claims.
	p.Audience = claims["aud"].(string)
	p.ExpiresAt = int64(claims["exp"].(float64))
	p.JTI = claims["jti"].(string)
	p.IssuedAt = int64(claims["iat"].(float64))
	p.Issuer = claims["iss"].(string)
	p.NotBefore = int64(claims["nbf"].(float64))
	p.Subject = claims["sub"].(string)

	p.Scopes = strings.Split(claims["Scopes"].(string), ",")

	// Custom claims.
	if claims["UserID"] != nil {
		userClaims := &jwt2.UserJWTClaims{
			UserID: claims["UserID"].(string),
			OrgID:  claims["OrgID"].(string),
			Email:  claims["Email"].(string),
		}
		p.CustomClaims = &jwt2.JWTClaims_UserClaims{
			UserClaims: userClaims,
		}
	} else if claims["ServiceID"] != nil {
		serviceClaims := &jwt2.ServiceJWTClaims{
			ServiceID: claims["ServiceID"].(string),
		}
		p.CustomClaims = &jwt2.JWTClaims_ServiceClaims{
			ServiceClaims: serviceClaims,
		}
	} else if claims["ClusterID"] != nil {
		clusterClaims := &jwt2.ClusterJWTClaims{
			ClusterID: claims["ClusterID"].(string),
		}
		p.CustomClaims = &jwt2.JWTClaims_ClusterClaims{
			ClusterClaims: clusterClaims,
		}
	}

	return p
}

// GenerateJWTForUser creates a protobuf claims for the given user.
func GenerateJWTForUser(userID string, orgID string, email string, expiresAt time.Time) *jwt2.JWTClaims {
	claims := jwt2.JWTClaims{
		Subject: userID,
		// Standard claims.
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
func GenerateJWTForService(serviceID string) *jwt2.JWTClaims {
	pbClaims := jwt2.JWTClaims{
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
func GenerateJWTForCluster(clusterID string) *jwt2.JWTClaims {
	pbClaims := jwt2.JWTClaims{
		Audience:  "pixielabs.ai",
		ExpiresAt: time.Now().Add(time.Hour).Unix(),
		IssuedAt:  time.Now().Unix(),
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
