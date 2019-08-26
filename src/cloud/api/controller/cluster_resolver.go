package controller

import (
	"context"
	"errors"

	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"

	cloudpb "pixielabs.ai/pixielabs/src/cloud/cloudpb"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
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

	return &ClusterInfoResolver{clusterID: u}, nil
}

// ClusterResolver is the resolver responsible for clusters belonging to the given org.
type ClusterResolver struct {
	SessionCtx *authcontext.AuthContext
}

// Cluster resolves cluster information.
func (q *QueryResolver) Cluster(ctx context.Context) (*ClusterInfoResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	orgIDstr := sCtx.Claims.OrgID

	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	viziers, err := q.Env.VZMgrClient().GetViziersByOrg(ctx, orgID)
	if err != nil {
		return nil, err
	}

	if len(viziers.VizierIDs) == 0 {
		return nil, errors.New("org has no clusters")
	}
	// Take first ID for now.
	info, err := q.Env.VZMgrClient().GetVizierInfo(ctx, viziers.VizierIDs[0])
	if err != nil {
		return nil, err
	}

	clusterID, err := utils.UUIDFromProto(info.VizierID)
	if err != nil {
		return nil, err
	}

	return &ClusterInfoResolver{
		clusterID, info.Status, float64(info.LastHeartbeatNs),
	}, nil
}

// ClusterInfoResolver is the resolver responsible for cluster info.
type ClusterInfoResolver struct {
	clusterID       uuid.UUID
	status          cloudpb.VizierInfo_Status
	lastHeartbeatNs float64
}

// ID returns cluster ID.
func (c *ClusterInfoResolver) ID() graphql.ID {
	return graphql.ID(c.clusterID.String())
}

// Status returns the cluster status.
func (c *ClusterInfoResolver) Status() string {
	return c.status.String()
}

// LastHeartbeatMs returns the heartbeat.
func (c *ClusterInfoResolver) LastHeartbeatMs() float64 {
	return float64(c.lastHeartbeatNs / 1E6)
}
