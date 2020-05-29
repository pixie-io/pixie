package utils

import (
	"strings"

	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/shared/services"
)

// GetCloudClientConnection gets the GRPC connection based on the cloud addr.
func GetCloudClientConnection(cloudAddr string) (*grpc.ClientConn, error) {
	isInternal := strings.ContainsAny(cloudAddr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	c, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return c, nil
}
