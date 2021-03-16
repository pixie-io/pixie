package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"pixielabs.ai/pixielabs/src/cloud/project_manager/projectmanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
)

func init() {
	pflag.String("project_manager_service", "kubernetes:///project-manager-service.plc:50300", "The projectmanager service url (load balancer/list is ok)")
}

// NewProjectManagerServiceClient creates a new auth RPC client stub.
func NewProjectManagerServiceClient() (projectmanagerpb.ProjectManagerServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	projectManagerChannel, err := grpc.Dial(viper.GetString("project_manager_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return projectmanagerpb.NewProjectManagerServiceClient(projectManagerChannel), nil
}
