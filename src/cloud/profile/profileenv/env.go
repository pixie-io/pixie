package profileenv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
)

func init() {
	pflag.String("project_manager_service", "project-manager-service.plc.svc.cluster.local:50300", "The project manager service url (load balancer/list is ok)")
}

// ProfileEnv is the environment used for the profile service.
type ProfileEnv interface {
	env.Env
	ProjectManagerClient() projectmanagerpb.ProjectManagerServiceClient
}

// Impl is an implementation of the AuthEnv interface
type Impl struct {
	*env.BaseEnv
	projectManagerClient projectmanagerpb.ProjectManagerServiceClient
}

// ProjectManagerClient is an accessor for the project manager client.
func (p *Impl) ProjectManagerClient() projectmanagerpb.ProjectManagerServiceClient {
	return p.projectManagerClient
}

// NewWithDefaults creates a profile env with the default clients and values.
func NewWithDefaults() (*Impl, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}
	projectChannel, err := grpc.Dial(viper.GetString("project_manager_service"), dialOpts...)
	if err != nil {
		return nil, err
	}
	pc := projectmanagerpb.NewProjectManagerServiceClient(projectChannel)
	return New(pc), nil
}

// New creates a new profile env.
func New(pm projectmanagerpb.ProjectManagerServiceClient) *Impl {
	return &Impl{env.New(viper.GetString("domain_name")), pm}
}
