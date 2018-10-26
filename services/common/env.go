package common

import (
	"fmt"

	"github.com/dgrijalva/jwt-go"
	pb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/services/common/utils"
)

type ctxKey string

// EnvKey is the context key used for the environment.
const EnvKey ctxKey = "Env"

// Env is the struct containing server state that might be relevant to
// executing a particualar API call (ie. DB to lookup user information).
type Env struct {
	ExternalAddress string
	SigningKey      string
	Claims          *pb.JWTClaims
}

// BaseEnver is the interface that all sub-environments should implement.
type BaseEnver interface {
	GetBaseEnv() *Env
	SetClaims(claims *pb.JWTClaims)
}

// ParseJWTClaims takes a token and returns a JwtClaims.
func (e *Env) ParseJWTClaims(tokenString string) (*pb.JWTClaims, error) {
	secret := e.SigningKey
	token, err := jwt.ParseWithClaims(tokenString, &jwt.MapClaims{}, func(token *jwt.Token) (interface{}, error) {
		// validate that the signig method is correct
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("Unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(secret), nil
	})
	if err != nil {
		return nil, err
	}

	claims := token.Claims.(*jwt.MapClaims)
	pbClaims := utils.MapClaimsToPB(*claims)
	return pbClaims, err
}

// SetClaims sets the JWT claims.
func (e *Env) SetClaims(claims *pb.JWTClaims) {
	e.Claims = claims
}

// GetBaseEnv returns the pointer to the base environment (in this case itself).
func (e *Env) GetBaseEnv() *Env {
	return e
}
