package testingutils

import (
	"testing"

	"github.com/dgrijalva/jwt-go"
	pb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/services/common/utils"
)

// GenerateTestJWTToken generates valid tokens for testing.
func GenerateTestJWTToken(t *testing.T, signingKey string) string {
	claims := pb.JWTClaims{}
	claims.UserID = "test"
	claims.Email = "test@test.com"
	claims.Issuer = "PL"

	mc := utils.PBToMapClaims(&claims)
	token, err := jwt.NewWithClaims(jwt.SigningMethodHS256, mc).SignedString([]byte(signingKey))
	if err != nil {
		t.Fatal("failed to generate token")
	}
	return token
}
