package utils

import (
	"github.com/dgrijalva/jwt-go"
	pb "pixielabs.ai/pixielabs/services/common/proto"
)

// PBToMapClaims maps protobuf claims to map claims.
func PBToMapClaims(pb *pb.JWTClaims) jwt.MapClaims {
	claims := jwt.MapClaims{}

	// Standard claims.
	claims["aud"] = pb.Audience
	claims["exp"] = pb.ExpiresAt
	claims["jti"] = pb.ID
	claims["iat"] = pb.IssuedAt
	claims["iss"] = pb.Issuer
	claims["nbf"] = pb.NotBefore
	claims["sub"] = pb.Subject

	// Custom claims.
	claims["UserID"] = pb.UserID
	claims["Email"] = pb.Email

	return claims
}
