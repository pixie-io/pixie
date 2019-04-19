package apienv

import (
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/services/common/env"
	service "pixielabs.ai/pixielabs/src/vizier/proto"
)

const defaultVizierAddr = "localhost:40000"

// APIEnv is the interface for the API service environment.
type APIEnv interface {
	env.Env
	VizierClient() service.VizierServiceClient
}

// Impl is an implementation of the ApiEnv interface
type Impl struct {
	*env.BaseEnv
	Conn   *grpc.ClientConn
	Client service.VizierServiceClient
}

// New creates a new api env.
func New() (*Impl, error) {
	conn, err := grpc.Dial(defaultVizierAddr, grpc.WithInsecure())
	if err != nil {
		return nil, err
	}
	srvc := service.NewVizierServiceClient(conn)
	return &Impl{env.New(), conn, srvc}, nil
}

// VizierClient returns a GRPC vizier client.
func (c *Impl) VizierClient() service.VizierServiceClient {
	return c.Client
}
