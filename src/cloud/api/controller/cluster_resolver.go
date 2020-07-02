package controller

import (
	"context"
	"errors"

	types "github.com/gogo/protobuf/types"
	"github.com/graph-gophers/graphql-go"
	uuid "github.com/satori/go.uuid"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

// CreateCluster creates a new cluster.
func (q *QueryResolver) CreateCluster(ctx context.Context) (*ClusterInfoResolver, error) {
	return nil, errors.New("Deprecated. Please use `px deploy`")
}

type clusterArgs struct {
	ID *graphql.ID
}

func clusterInfoToResolver(cluster *cloudapipb.ClusterInfo) (*ClusterInfoResolver, error) {
	clusterID, err := utils.UUIDFromProto(cluster.ID)
	if err != nil {
		return nil, err
	}

	return &ClusterInfoResolver{
		clusterID, cluster.Status, float64(cluster.LastHeartbeatNs), &VizierConfigResolver{
			passthroughEnabled: &cluster.Config.PassthroughEnabled,
		},
		&cluster.VizierVersion,
		&cluster.ClusterVersion,
		&cluster.ClusterUID,
		&cluster.ClusterName,
		&cluster.PrettyClusterName,
	}, nil
}

// Clusters lists all of the clusters.
func (q *QueryResolver) Clusters(ctx context.Context) ([]*ClusterInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo
	resp, err := grpcAPI.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}

	var res []*ClusterInfoResolver
	for _, cluster := range resp.Clusters {
		resolver, err := clusterInfoToResolver(cluster)
		if err != nil {
			return nil, err
		}
		res = append(res, resolver)
	}
	return res, nil
}

// Cluster resolves cluster information.
func (q *QueryResolver) Cluster(ctx context.Context, args *clusterArgs) (*ClusterInfoResolver, error) {
	if args == nil || args.ID == nil {
		clusters, err := q.Clusters(ctx)
		if err != nil {
			return nil, err
		}
		if len(clusters) == 0 {
			return nil, errors.New("org has no clusters")
		}
		// Take first cluster for now.
		return clusters[0], nil
	}

	grpcAPI := q.Env.VizierClusterInfo
	res, err := grpcAPI.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(*(args.ID))),
	})
	if err != nil {
		return nil, err
	}
	if len(res.Clusters) == 0 {
		return nil, errors.New("org has no matching clusters")
	}
	if len(res.Clusters) != 1 {
		return nil, errors.New("got multiple matching clusters for ID")
	}
	return clusterInfoToResolver(res.Clusters[0])
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
	grpcAPI := q.Env.VizierClusterInfo

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
	clusterID         uuid.UUID
	status            cloudapipb.ClusterStatus
	lastHeartbeatNs   float64
	vizierConfig      *VizierConfigResolver
	vizierVersion     *string
	clusterVersion    *string
	clusterUID        *string
	clusterName       *string
	prettyClusterName *string
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
	return float64(c.lastHeartbeatNs) / 1e6
}

// VizierConfig returns the config for the Vizier.
func (c *ClusterInfoResolver) VizierConfig() *VizierConfigResolver {
	return c.vizierConfig
}

// ClusterVersion returns the k8s cluster version.
func (c *ClusterInfoResolver) ClusterVersion() *string {
	return c.clusterVersion
}

// ClusterUID returns the k8s cluster UID.
func (c *ClusterInfoResolver) ClusterUID() *string {
	return c.clusterUID
}

// ClusterName returns the k8s cluster name.
func (c *ClusterInfoResolver) ClusterName() *string {
	return c.clusterName
}

// PrettyClusterName returns the k8s cluster name prettified.
func (c *ClusterInfoResolver) PrettyClusterName() *string {
	return c.prettyClusterName
}

// VizierVersion returns the vizier's version.
func (c *ClusterInfoResolver) VizierVersion() *string {
	return c.vizierVersion
}

// ClusterConnection resolves cluster connection information.
// TODO(nserrino): When we have multiple clusters per customer, we will need to change this API to take
// a cluster ID argument to match the GRPC one.
func (q *QueryResolver) ClusterConnection(ctx context.Context, args *clusterArgs) (*ClusterConnectionInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo

	var clusterID *uuidpb.UUID
	if args == nil || args.ID == nil {
		resp, err := grpcAPI.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
		if err != nil {
			return nil, err
		}

		if len(resp.Clusters) == 0 {
			return nil, errors.New("org has no clusters")
		}

		// Take first ID for now.
		clusterID = resp.Clusters[0].ID
	} else {
		clusterID = utils.ProtoFromUUIDStrOrNil(string(*(args.ID)))
	}

	info, err := grpcAPI.GetClusterConnectionInfo(ctx, &cloudapipb.GetClusterConnectionInfoRequest{
		ID: clusterID,
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
