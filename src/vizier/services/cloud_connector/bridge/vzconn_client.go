package bridge

import (
	"context"
	"strings"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/shared/services"
)

func init() {
	pflag.String("cloud_addr", "vzconn-service.plc.svc:51600", "The Pixie Cloud service url (load balancer/list is ok)")
}

// NewVZConnClient creates a new vzconn RPC client stub.
func NewVZConnClient() (vzconnpb.VZConnServiceClient, error) {
	cloudAddr := viper.GetString("cloud_addr")

	isInternal := strings.ContainsAny(cloudAddr, ".svc.cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}
	dialOpts = append(dialOpts, []grpc.DialOption{grpc.WithBlock()}...)

	ctxBg := context.Background()
	ctx, cancel := context.WithTimeout(ctxBg, 10*time.Second)
	defer cancel()
	ccChannel, err := grpc.DialContext(ctx, cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzconnpb.NewVZConnServiceClient(ccChannel), nil
}
