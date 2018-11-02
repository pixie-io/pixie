package sessioncontext

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/dgrijalva/jwt-go"
	pb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/services/common/utils"
)

type sessionContextKey struct{}

// SessionContext stores sessions specific information.
type SessionContext struct {
	AuthToken string
	Claims    *pb.JWTClaims
}

// New creates a new sesion context.
func New() *SessionContext {
	return &SessionContext{}
}

// UseJWTAuth takes a token and sets claims, etc.
func (s *SessionContext) UseJWTAuth(signingKey string, tokenString string) error {
	secret := signingKey
	token, err := jwt.ParseWithClaims(tokenString, &jwt.MapClaims{}, func(token *jwt.Token) (interface{}, error) {
		// validate that the signing method is correct
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("Unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(secret), nil
	})
	if err != nil {
		return err
	}

	claims := token.Claims.(*jwt.MapClaims)
	s.Claims = utils.MapClaimsToPB(*claims)
	s.AuthToken = tokenString
	return nil
}

// ValidUser returns true if the user is logged in and valid.
func (s *SessionContext) ValidUser() bool {
	if s.Claims == nil {
		return false
	}

	if len(s.Claims.Subject) > 0 &&
		len(s.Claims.UserID) > 0 &&
		s.Claims.ExpiresAt > time.Now().Unix() {
		return true
	}

	return false
}

// NewContext returns a new context with session context.
func NewContext(ctx context.Context, s *SessionContext) context.Context {
	return context.WithValue(ctx, sessionContextKey{}, s)
}

// FromContext returns a session context from the passed in Context.
func FromContext(ctx context.Context) (*SessionContext, error) {
	s, ok := ctx.Value(sessionContextKey{}).(*SessionContext)
	if !ok {
		return nil, errors.New("failed to get session info from context")
	}
	return s, nil
}
