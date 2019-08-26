package controller

import (
	"context"

	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/utils"
)

type createClusterArgs struct {
	DomainName *string
}

// CreateCluster creates a new cluster.
func (q *QueryResolver) CreateCluster(ctx context.Context, args *createClusterArgs) (*ClusterInfoResolver, error) {
	apiEnv := q.Env

	orgReq := &profilepb.GetOrgByDomainRequest{
		DomainName: *args.DomainName,
	}

	resp, err := apiEnv.ProfileClient().GetOrgByDomain(ctx, orgReq)
	if err != nil {
		return nil, err
	}

	clusterReq := &vzmgrpb.CreateVizierClusterRequest{
		OrgID: resp.ID,
	}

	id, err := apiEnv.VZMgrClient().CreateVizierCluster(ctx, clusterReq)
	if err != nil {
		return nil, err
	}

	u, err := utils.UUIDFromProto(id)
	if err != nil {
		return nil, err
	}

	return &ClusterInfoResolver{u}, nil
}

// ClusterInfoResolver is the resolver responsible for cluster info.
type ClusterInfoResolver struct {
	clusterID uuid.UUID
}

// ID returns cluster ID.
func (c *ClusterInfoResolver) ID() graphql.ID {
	return graphql.ID(c.clusterID.String())
}
