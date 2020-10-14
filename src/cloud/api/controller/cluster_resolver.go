package controller

import (
	"context"
	"errors"
	"sort"

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

func timestampProtoToNanos(ts *types.Timestamp) int64 {
	return ts.Seconds*NanosPerSecond + int64(ts.Nanos)
}

func containerStatusToResolver(containerStatus *cloudapipb.ContainerStatus) (*ContainerStatusResolver, error) {
	if containerStatus == nil {
		return nil, errors.New("got nil container status")
	}

	resolver := &ContainerStatusResolver{
		name:    containerStatus.Name,
		state:   containerStatus.State.String(),
		message: &containerStatus.Message,
		reason:  &containerStatus.Reason,
	}

	if containerStatus.CreatedAt != nil {
		resolver.createdAtNS = timestampProtoToNanos(containerStatus.CreatedAt)
	}
	return resolver, nil
}

func podStatusToResolver(podStatus *cloudapipb.PodStatus) (*PodStatusResolver, error) {
	if podStatus == nil {
		return nil, errors.New("got nil pod status")
	}

	var containers []*ContainerStatusResolver
	for _, containerStatus := range podStatus.Containers {
		c, err := containerStatusToResolver(containerStatus)
		if err != nil {
			return nil, err
		}
		containers = append(containers, c)
	}

	var events []*K8sEventResolver
	for _, ev := range podStatus.Events {
		e, err := k8sEventToResolver(ev)
		if err != nil {
			return nil, err
		}
		events = append(events, e)
	}

	resolver := &PodStatusResolver{
		name:       podStatus.Name,
		status:     podStatus.Status.String(),
		message:    &podStatus.StatusMessage,
		reason:     &podStatus.Reason,
		containers: containers,
		events:     events,
	}

	if podStatus.CreatedAt != nil {
		resolver.createdAtNS = timestampProtoToNanos(podStatus.CreatedAt)
	}
	return resolver, nil
}

func k8sEventToResolver(event *cloudapipb.K8SEvent) (*K8sEventResolver, error) {
	if event == nil {
		return nil, errors.New("got nil k8s event")
	}

	resolver := &K8sEventResolver{
		message: &event.Message,
	}

	if event.FirstTime != nil {
		resolver.firstTimeNS = timestampProtoToNanos(event.FirstTime)
	}
	if event.LastTime != nil {
		resolver.lastTimeNS = timestampProtoToNanos(event.LastTime)
	}

	return resolver, nil
}

func clusterInfoToResolver(cluster *cloudapipb.ClusterInfo) (*ClusterInfoResolver, error) {
	clusterID, err := utils.UUIDFromProto(cluster.ID)
	if err != nil {
		return nil, err
	}

	var podStatuses []*PodStatusResolver
	for _, podStatus := range cluster.ControlPlanePodStatuses {
		p, err := podStatusToResolver(podStatus)
		if err != nil {
			return nil, err
		}
		podStatuses = append(podStatuses, p)
	}
	sort.SliceStable(podStatuses, func(i, j int) bool {
		return podStatuses[i].name < podStatuses[j].name
	})

	return &ClusterInfoResolver{
		clusterID:       clusterID,
		status:          cluster.Status,
		lastHeartbeatNs: float64(cluster.LastHeartbeatNs),
		vizierConfig: &VizierConfigResolver{
			passthroughEnabled: &cluster.Config.PassthroughEnabled,
		},
		vizierVersion:           &cluster.VizierVersion,
		clusterVersion:          &cluster.ClusterVersion,
		clusterUID:              &cluster.ClusterUID,
		clusterName:             &cluster.ClusterName,
		prettyClusterName:       &cluster.PrettyClusterName,
		controlPlanePodStatuses: podStatuses,
		numNodes:                cluster.NumNodes,
		numInstrumentedNodes:    cluster.NumInstrumentedNodes,
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

// ContainerStatusResolver is the resolver responsible for container status info.
type ContainerStatusResolver struct {
	name        string
	createdAtNS int64
	state       string
	message     *string
	reason      *string
}

// Name returns container name.
func (c *ContainerStatusResolver) Name() string {
	return c.name
}

// CreatedAtMs returns the container create time.
func (c *ContainerStatusResolver) CreatedAtMs() float64 {
	return float64(c.createdAtNS) / 1e6
}

// State returns container state.
func (c *ContainerStatusResolver) State() string {
	return c.state
}

// Message returns container state message.
func (c *ContainerStatusResolver) Message() *string {
	return c.message
}

// Reason returns container state reason.
func (c *ContainerStatusResolver) Reason() *string {
	return c.reason
}

// PodStatusResolver is the resolver responsible for pod status info.
type PodStatusResolver struct {
	name        string
	createdAtNS int64
	status      string
	message     *string
	reason      *string
	containers  []*ContainerStatusResolver
	events      []*K8sEventResolver
}

// Name returns pod name.
func (p *PodStatusResolver) Name() string {
	return p.name
}

// CreatedAtMs returns the pod create time.
func (p *PodStatusResolver) CreatedAtMs() float64 {
	return float64(p.createdAtNS) / 1e6
}

// Status returns pod status.
func (p *PodStatusResolver) Status() string {
	return p.status
}

// Message returns pod message.
func (p *PodStatusResolver) Message() *string {
	return p.message
}

// Reason returns pod reason.
func (p *PodStatusResolver) Reason() *string {
	return p.reason
}

// Containers returns pod container states.
func (p *PodStatusResolver) Containers() []*ContainerStatusResolver {
	return p.containers
}

// Events returns the events belonging to the pod.
func (p *PodStatusResolver) Events() []*K8sEventResolver {
	return p.events
}

// ClusterInfoResolver is the resolver responsible for cluster info.
type ClusterInfoResolver struct {
	clusterID               uuid.UUID
	status                  cloudapipb.ClusterStatus
	lastHeartbeatNs         float64
	vizierConfig            *VizierConfigResolver
	vizierVersion           *string
	clusterVersion          *string
	clusterUID              *string
	clusterName             *string
	prettyClusterName       *string
	controlPlanePodStatuses []*PodStatusResolver
	numNodes                int32
	numInstrumentedNodes    int32
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

// ControlPlanePodStatuses returns the control plane pod statuses.
func (c *ClusterInfoResolver) ControlPlanePodStatuses() []*PodStatusResolver {
	return c.controlPlanePodStatuses
}

// NumNodes returns the number of nodes on the cluster.
func (c *ClusterInfoResolver) NumNodes() int32 {
	return c.numNodes
}

// NumInstrumentedNodes returns the number of nodes with pems on the cluster.
func (c *ClusterInfoResolver) NumInstrumentedNodes() int32 {
	return c.numInstrumentedNodes
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

// K8sEventResolver is a resolver for k8s events.
type K8sEventResolver struct {
	message     *string
	firstTimeNS int64
	lastTimeNS  int64
}

// FirstTimeMs returns the timestamp of when the event first occurred.
func (k *K8sEventResolver) FirstTimeMs() float64 {
	return float64(k.firstTimeNS) / 1e6
}

// LastTimeMs returns the timestamp of when the event last occurred.
func (k *K8sEventResolver) LastTimeMs() float64 {
	return float64(k.lastTimeNS) / 1e6
}

// Message returns the event message.
func (k *K8sEventResolver) Message() *string {
	return k.message
}
