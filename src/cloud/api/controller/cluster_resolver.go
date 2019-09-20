package controller

import (
	"context"
	"errors"

	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"

	cloudpb "pixielabs.ai/pixielabs/src/cloud/cloudpb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils"
)

// CreateCluster creates a new cluster.
func (q *QueryResolver) CreateCluster(ctx context.Context) (*ClusterInfoResolver, error) {
	apiEnv := q.Env

	sCtx, err := authcontext.FromContext(ctx)
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID

	orgID := utils.ProtoFromUUIDStrOrNil(orgIDstr)

	clusterReq := &vzmgrpb.CreateVizierClusterRequest{
		OrgID: orgID,
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
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID

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
	return float64(c.lastHeartbeatNs / 1e6)
}

// ClusterConnection resolves cluster connection information.
func (q *QueryResolver) ClusterConnection(ctx context.Context) (*ClusterConnectionInfoResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID

	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	viziers, err := q.Env.VZMgrClient().GetViziersByOrg(ctx, utils.ProtoFromUUID(&orgID))
	if err != nil {
		return nil, err
	}

	if len(viziers.VizierIDs) == 0 {
		return nil, errors.New("org has no clusters")
	}
	// Take first ID for now.
	info, err := q.Env.VZMgrClient().GetVizierConnectionInfo(ctx, viziers.VizierIDs[0])
	if err != nil {
		return nil, err
	}

	return &ClusterConnectionInfoResolver{
		info.IPAddress,
		info.Token,
	}, nil
}

// ClusterConnectionInfoResolver is the resolver responsible for cluster connection info.
type ClusterConnectionInfoResolver struct {
	ipAddress string
	token     string
}

// IPAddress returns the connection's IP.
func (c *ClusterConnectionInfoResolver) IPAddress() string {
	return c.ipAddress
}

// Token returns the connection's token.
func (c *ClusterConnectionInfoResolver) Token() string {
	return c.token
}
