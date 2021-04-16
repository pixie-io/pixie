package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("vzmgr_service", "vzmgr-service.plc.svc.cluster.local:51800", "The vzmgr service url (load balancer/list is ok)")
}

// NewVZMgrServiceClients creates the vzmgr RPC client stubs.
func NewVZMgrServiceClients() (vzmgrpb.VZMgrServiceClient, vzmgrpb.VZDeploymentKeyServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, nil, err
	}

	vzMgrChan, err := grpc.Dial(viper.GetString("vzmgr_service"), dialOpts...)
	if err != nil {
		return nil, nil, err
	}

	return vzmgrpb.NewVZMgrServiceClient(vzMgrChan), vzmgrpb.NewVZDeploymentKeyServiceClient(vzMgrChan), nil
}
