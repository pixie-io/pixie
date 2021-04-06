package authcontext

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/dgrijalva/jwt-go/v4"
	"github.com/gofrs/uuid"

	jwt2 "pixielabs.ai/pixielabs/src/shared/services/proto"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
)

type authContextKey struct{}

// AuthContext stores sessions specific information.
type AuthContext struct {
	AuthToken string
	Claims    *jwt2.JWTClaims
	Path      string
}

// New creates a new sesion context.
func New() *AuthContext {
	return &AuthContext{}
}

// UseJWTAuth takes a token and sets claims, etc.
func (s *AuthContext) UseJWTAuth(signingKey string, tokenString string, audience string) error {
	secret := signingKey
	token, err := jwt.ParseWithClaims(tokenString, &jwt.MapClaims{}, func(token *jwt.Token) (interface{}, error) {
		// validate that the signing method is correct
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("Unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(secret), nil
	}, jwt.WithAudience(audience))
	if err != nil {
		return err
	}

	claims := token.Claims.(*jwt.MapClaims)
	s.Claims, err = utils.MapClaimsToPB(*claims)
	if err != nil {
		return err
	}
	s.AuthToken = tokenString
	return nil
}

// ValidClaims returns true if the user is logged in and valid.
func (s *AuthContext) ValidClaims() bool {
	if s.Claims == nil {
		return false
	}

	if len(s.Claims.Subject) == 0 {
		return false
	}
	if s.Claims.ExpiresAt < time.Now().Unix() {
		return false
	}

	switch utils.GetClaimsType(s.Claims) {
	case utils.UserClaimType:
		return s.Claims.GetUserClaims() != nil && len(s.Claims.GetUserClaims().UserID) > 0
	case utils.ServiceClaimType:
		return s.Claims.GetServiceClaims() != nil && len(s.Claims.GetServiceClaims().ServiceID) > 0
	case utils.ClusterClaimType:
		clusterClaims := s.Claims.GetClusterClaims()
		if clusterClaims == nil {
			return false
		}
		clusterClaimID := uuid.FromStringOrNil(clusterClaims.ClusterID)
		return clusterClaimID != uuid.Nil
	default:
	}
	return false
}

// NewContext returns a new context with session context.
func NewContext(ctx context.Context, s *AuthContext) context.Context {
	return context.WithValue(ctx, authContextKey{}, s)
}

// FromContext returns a session context from the passed in Context.
func FromContext(ctx context.Context) (*AuthContext, error) {
	s, ok := ctx.Value(authContextKey{}).(*AuthContext)
	if !ok {
		return nil, errors.New("failed to get auth info from context")
	}
	return s, nil
}
