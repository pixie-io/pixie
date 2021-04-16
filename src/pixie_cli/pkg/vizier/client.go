package vizier

import (
	"context"
	"fmt"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/cloud/cloudapipb"
	"px.dev/pixie/src/shared/services"
)

func ctxWithTokenCreds(ctx context.Context, token string) context.Context {
	return metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", token))
}

func newVizierClusterInfoClient(cloudAddr string) (cloudapipb.VizierClusterInfoClient, error) {
	isInternal := strings.ContainsAny(cloudAddr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	c, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return cloudapipb.NewVizierClusterInfoClient(c), nil
}
