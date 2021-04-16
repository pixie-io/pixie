package authenv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	profilepb "px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services"
)

func init() {
	pflag.String("profile_service", "profile-service.plc.svc.cluster.local:51500", "The profilepb service url (load balancer/list is ok)")
}

func newProfileServiceClient() (profilepb.ProfileServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	profileChannel, err := grpc.Dial(viper.GetString("profile_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return profilepb.NewProfileServiceClient(profileChannel), nil
}
