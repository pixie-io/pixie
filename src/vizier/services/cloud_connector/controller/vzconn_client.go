package controllers

import (
	"fmt"
	"strings"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/shared/services"
)

func init() {
	pflag.String("cloud_addr", "vzconn-service.plc.svc:51600", "The Pixie Cloud service url (load balancer/list is ok)")
	pflag.String("dev_cloud_namespace", "", "The namespace of Pixie Cloud running on minikube.")
}

// NewVZConnClient creates a new vzconn RPC client stub.
func NewVZConnClient() (vzconnpb.VZConnServiceClient, error) {
	cloudAddr := viper.GetString("cloud_addr")
	devCloudNamespace := viper.GetString("dev_cloud_namespace")

	// If attempting to connect to a cloud instance running on minikube, use the FQDN.
	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
	}
	isInternal := strings.ContainsAny(cloudAddr, ".svc.cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	ccChannel, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzconnpb.NewVZConnServiceClient(ccChannel), nil
}
