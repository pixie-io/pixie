package cmd

import (
	"context"
	"errors"
	"fmt"

	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/metadata"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
)

// getClusterInfo is a placeholder to fetch the cluster ID to query/install.
// This will be replaced with a more comprehensive config/cluster management in future releases
func getClusterInfo(cloudAddr string) (*cloudapipb.ClusterInfo, *uuid.UUID, error) {
	// Get grpc connection to cloud.

	conn, err := getCloudClientConnection(cloudAddr)
	if err != nil {
		return nil, nil, err
	}

	client := cloudapipb.NewVizierClusterInfoClient(conn)

	creds, err := auth.LoadDefaultCredentials()
	if err != nil {
		return nil, nil, err
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", creds.Token))
	resp, err := client.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, nil, err
	}
	if len(resp.Clusters) != 1 {
		return nil, nil, errors.New("currently on a single cluster is allowed")
	}
	selectedCluster := resp.Clusters[0]
	id := utils.UUIDFromProtoOrNil(selectedCluster.ID)
	return selectedCluster, &id, nil
}
