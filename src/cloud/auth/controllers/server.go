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

// Server defines an gRPC server type.
type Server struct {
	env       authenv.AuthEnv
	a         Auth0Connector
	apiKeyMgr APIKeyMgr
}

// NewServer creates GRPC handlers.
func NewServer(env authenv.AuthEnv, a Auth0Connector, apiKeyMgr APIKeyMgr) (*Server, error) {
	return &Server{
		env:       env,
		a:         a,
		apiKeyMgr: apiKeyMgr,
	}, nil
}
