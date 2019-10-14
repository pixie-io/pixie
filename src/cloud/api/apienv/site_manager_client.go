package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
)

func init() {
	pflag.String("site_manager_service", "kubernetes:///site-manager-service.plc:50300", "The sitemanager service url (load balancer/list is ok)")
}

// NewSiteManagerServiceClient creates a new auth RPC client stub.
func NewSiteManagerServiceClient() (sitemanagerpb.SiteManagerServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	siteManagerChannel, err := grpc.Dial(viper.GetString("site_manager_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return sitemanagerpb.NewSiteManagerServiceClient(siteManagerChannel), nil
}
