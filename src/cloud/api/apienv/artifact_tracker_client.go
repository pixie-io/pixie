package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("artifact_tracker_service", "kubernetes:///artifact-tracker-service.plc:50750", "The artifact tracker service url (load balancer/list is ok)")
}

// NewArtifactTrackerClient creates a new artifact tracker RPC client stub.
func NewArtifactTrackerClient() (artifacttrackerpb.ArtifactTrackerClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	atChannel, err := grpc.Dial(viper.GetString("artifact_tracker_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return artifacttrackerpb.NewArtifactTrackerClient(atChannel), nil
}
