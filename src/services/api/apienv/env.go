package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/services/common/env"
	qbpb "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

func init() {
	pflag.String("query_broker_grpc_addr",
		"vizier-query-broker.pl.svc.cluster.local:50300",
		"The address to the query broker grpc server")
}

// APIEnv is the interface for the API service environment.
type APIEnv interface {
	env.Env
	QueryBrokerClient() qbpb.QueryBrokerServiceClient
}

// Impl is an implementation of the ApiEnv interface
type Impl struct {
	*env.BaseEnv
	Conn   *grpc.ClientConn
	Client qbpb.QueryBrokerServiceClient
}

// New creates a new api env.
func New() (*Impl, error) {
	dialOpts, err := common.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}
	dialOpts = append(dialOpts, grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(20000000)))
	conn, err := grpc.Dial(viper.GetString("query_broker_grpc_addr"), dialOpts...)
	if err != nil {
		return nil, err
	}
	srvc := qbpb.NewQueryBrokerServiceClient(conn)
	return &Impl{env.New(), conn, srvc}, nil
}

// QueryBrokerClient returns a GRPC vizier client.
func (c *Impl) QueryBrokerClient() qbpb.QueryBrokerServiceClient {
	return c.Client
}
