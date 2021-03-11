package controllers

import (
	"context"

	uuid "github.com/satori/go.uuid"
	"pixielabs.ai/pixielabs/src/cloud/auth/authenv"
)

// APIKeyMgr is the internal interface for managing API keys.
type APIKeyMgr interface {
	FetchOrgUserIDUsingAPIKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, error)
}

// AuthProvider interfaces the service we use for auth.
type AuthProvider interface {
	// GetUserIDFromToken returns the UserID for the particular auth token.
	GetUserIDFromToken(token string) (string, error)
	// GetUserInfo returns the UserInfo for the userID.
	GetUserInfo(userID string) (*UserInfo, error)
	// SetPLMetadata sets the pixielabs related metadata in the auth provider.
	SetPLMetadata(userID, plOrgID, plUserID string) error
}

// Server defines an gRPC server type.
type Server struct {
	env       authenv.AuthEnv
	a         AuthProvider
	apiKeyMgr APIKeyMgr
}

// NewServer creates GRPC handlers.
func NewServer(env authenv.AuthEnv, a AuthProvider, apiKeyMgr APIKeyMgr) (*Server, error) {
	return &Server{
		env:       env,
		a:         a,
		apiKeyMgr: apiKeyMgr,
	}, nil
}
