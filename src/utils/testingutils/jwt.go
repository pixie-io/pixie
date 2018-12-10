package testingutils

import (
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	pb "pixielabs.ai/pixielabs/src/services/common/proto"
	"pixielabs.ai/pixielabs/src/services/common/utils"
)

// GenerateTestClaims generates valid test user claims.
func GenerateTestClaims(t *testing.T) *pb.JWTClaims {
	claims := pb.JWTClaims{}
	claims.Subject = "test"
	claims.UserID = "test"
	claims.Email = "test@test.com"
	claims.Issuer = "PL"
	claims.ExpiresAt = time.Now().Add(time.Minute * 60).Unix()

	return &claims
}

// GenerateTestJWTToken generates valid tokens for testing.
func GenerateTestJWTToken(t *testing.T, signingKey string) string {
	claims := GenerateTestClaims(t)

	return SignPBClaims(t, claims, signingKey)
}

// SignPBClaims signs our protobuf claims after converting to json.
func SignPBClaims(t *testing.T, claims *pb.JWTClaims, signingKey string) string {
	mc := utils.PBToMapClaims(claims)
	token, err := jwt.NewWithClaims(jwt.SigningMethodHS256, mc).SignedString([]byte(signingKey))
	if err != nil {
		t.Fatal("failed to generate token")
	}
	return token
}
