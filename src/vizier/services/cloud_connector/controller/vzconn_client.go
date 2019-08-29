package controllers

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/shared/services"
)

func init() {
	pflag.String("cloud_addr", "vzconn-service.plc.svc.cluster.local:51600", "The Pixie Cloud service url (load balancer/list is ok)")
}

// NewVZConnClient creates a new vzconn RPC client stub.
func NewVZConnClient() (vzconnpb.VZConnServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	ccChannel, err := grpc.Dial(viper.GetString("cloud_addr"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzconnpb.NewVZConnServiceClient(ccChannel), nil
}
