package controller

import (
	"context"
	"errors"

	types "github.com/gogo/protobuf/types"
	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils"
)

// CreateCluster creates a new cluster.
func (q *QueryResolver) CreateCluster(ctx context.Context) (*ClusterInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterServer
	res, err := grpcAPI.CreateCluster(ctx, &cloudapipb.CreateClusterRequest{})
	if err != nil {
		return nil, err
	}

	u, err := utils.UUIDFromProto(res.ClusterID)
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
	grpcAPI := q.Env.VizierClusterServer
	res, err := grpcAPI.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	if len(res.Clusters) == 0 {
		return nil, errors.New("org has no clusters")
	}
	// Take first cluster for now.
	cluster := res.Clusters[0]
	clusterID, err := utils.UUIDFromProto(cluster.ID)
	if err != nil {
		return nil, err
	}

	return &ClusterInfoResolver{
		clusterID, cluster.Status, float64(cluster.LastHeartbeatNs), &VizierConfigResolver{
			passthroughEnabled: &cluster.Config.PassthroughEnabled,
		},
	}, nil
}

// VizierConfigResolver is the resolver responsible for config belonging to the given cluster.
type VizierConfigResolver struct {
	passthroughEnabled *bool
}

// PassthroughEnabled returns whether passthrough mode is enabled on the cluster
func (v *VizierConfigResolver) PassthroughEnabled() *bool {
	return v.passthroughEnabled
}

type updateVizierConfigArgs struct {
	ClusterID          graphql.ID
	PassthroughEnabled *bool
}

// UpdateVizierConfig updates the Vizier config of the input cluster
func (q *QueryResolver) UpdateVizierConfig(ctx context.Context, args *updateVizierConfigArgs) (bool, error) {
	grpcAPI := q.Env.VizierClusterServer

	req := &cloudapipb.UpdateClusterVizierConfigRequest{
		ID:           utils.ProtoFromUUIDStrOrNil(string(args.ClusterID)),
		ConfigUpdate: &cloudapipb.VizierConfigUpdate{},
	}

	if args.PassthroughEnabled != nil {
		req.ConfigUpdate.PassthroughEnabled = &types.BoolValue{Value: *args.PassthroughEnabled}
	}

	_, err := grpcAPI.UpdateClusterVizierConfig(ctx, req)
	if err != nil {
		return false, err
	}
	return true, nil
}

// ClusterInfoResolver is the resolver responsible for cluster info.
type ClusterInfoResolver struct {
	clusterID       uuid.UUID
	status          cloudapipb.ClusterStatus
	lastHeartbeatNs float64
	vizierConfig    *VizierConfigResolver
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

// VizierConfig returns the config for the Vizier.
func (c *ClusterInfoResolver) VizierConfig() *VizierConfigResolver {
	return c.vizierConfig
}

// ClusterConnection resolves cluster connection information.
// TODO(nserrino): When we have multiple clusters per customer, we will need to change this API to take
// a cluster ID argument to match the GRPC one.
func (q *QueryResolver) ClusterConnection(ctx context.Context) (*ClusterConnectionInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterServer

	resp, err := grpcAPI.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}

	if len(resp.Clusters) == 0 {
		return nil, errors.New("org has no clusters")
	}

	// Take first ID for now.
	cluster := resp.Clusters[0]

	info, err := grpcAPI.GetClusterConnectionInfo(ctx, &cloudapipb.GetClusterConnectionInfoRequest{
		ID: cluster.ID,
	})
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
