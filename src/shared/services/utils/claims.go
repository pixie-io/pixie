package utils

import (
	"github.com/dgrijalva/jwt-go"
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
	claims["UserID"] = pb.UserID
	claims["Email"] = pb.Email

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

	// Custom claims.
	p.UserID = claims["UserID"].(string)
	p.Email = claims["Email"].(string)

	return p
}
