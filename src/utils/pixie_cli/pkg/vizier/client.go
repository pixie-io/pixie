package vizier

import (
	"context"
	"fmt"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
)

func ctxWithTokenCreds(ctx context.Context, token string) context.Context {
	ctxWithCreds := metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", token))
	return ctxWithCreds
}

func ctxWithCreds(ctx context.Context) (context.Context, error) {
	creds, err := auth.MustLoadDefaultCredentials()
	if err != nil {
		return nil, err
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", creds.Token))
	return ctxWithCreds, nil
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
