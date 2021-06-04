/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controller

import (
	"context"
	"errors"
	"sort"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/utils"
)

// CreateCluster creates a new cluster.
func (q *QueryResolver) CreateCluster(ctx context.Context) (*ClusterInfoResolver, error) {
	return nil, errors.New("Deprecated. Please use `px deploy`")
}

type clusterArgs struct {
	ID graphql.ID
}

func timestampProtoToMillis(ts *types.Timestamp) float64 {
	return float64(ts.Seconds*NanosPerSecond+int64(ts.Nanos)) / 1e6
}

// ContainerStatusResolver is the resolver responsible for container status info.
type ContainerStatusResolver struct {
	Name        string
	CreatedAtMs float64
	State       string
	Message     string
	Reason      string
}

func containerStatusToResolver(containerStatus cloudpb.ContainerStatus) ContainerStatusResolver {
	resolver := ContainerStatusResolver{
		Name:    containerStatus.Name,
		State:   containerStatus.State.String(),
		Message: containerStatus.Message,
		Reason:  containerStatus.Reason,
	}

	if containerStatus.CreatedAt != nil {
		resolver.CreatedAtMs = timestampProtoToMillis(containerStatus.CreatedAt)
	}
	return resolver
}

// K8sEventResolver is a resolver for k8s events.
type K8sEventResolver struct {
	Message     string
	FirstTimeMs float64
	LastTimeMs  float64
}

func k8sEventToResolver(event cloudpb.K8SEvent) K8sEventResolver {
	resolver := K8sEventResolver{
		Message: event.Message,
	}

	if event.FirstTime != nil {
		resolver.FirstTimeMs = timestampProtoToMillis(event.FirstTime)
	}
	if event.LastTime != nil {
		resolver.LastTimeMs = timestampProtoToMillis(event.LastTime)
	}

	return resolver
}

// PodStatusResolver is the resolver responsible for pod status info.
type PodStatusResolver struct {
	Name        string
	CreatedAtMs float64
	Status      string
	Message     string
	Reason      string
	Containers  []ContainerStatusResolver
	Events      []K8sEventResolver
}

func podStatusToResolver(podStatus cloudpb.PodStatus) PodStatusResolver {
	var containers []ContainerStatusResolver
	for _, containerStatus := range podStatus.Containers {
		if containerStatus == nil {
			continue
		}
		c := containerStatusToResolver(*containerStatus)
		containers = append(containers, c)
	}

	var events []K8sEventResolver
	for _, ev := range podStatus.Events {
		if ev == nil {
			continue
		}
		e := k8sEventToResolver(*ev)
		events = append(events, e)
	}

	resolver := PodStatusResolver{
		Name:       podStatus.Name,
		Status:     podStatus.Status.String(),
		Message:    podStatus.StatusMessage,
		Reason:     podStatus.Reason,
		Containers: containers,
		Events:     events,
	}

	if podStatus.CreatedAt != nil {
		resolver.CreatedAtMs = timestampProtoToMillis(podStatus.CreatedAt)
	}
	return resolver
}

// VizierConfigResolver is the resolver responsible for config belonging to the given cluster.
type VizierConfigResolver struct {
	PassthroughEnabled bool
}

// ClusterInfoResolver is the resolver responsible for cluster info.
type ClusterInfoResolver struct {
	clusterID               uuid.UUID
	Status                  string
	LastHeartbeatMs         float64
	VizierConfig            VizierConfigResolver
	VizierVersion           string
	ClusterVersion          string
	ClusterUID              string
	ClusterName             string
	PrettyClusterName       string
	ControlPlanePodStatuses []PodStatusResolver
	NumNodes                int32
	NumInstrumentedNodes    int32
}

// ID returns cluster ID.
func (c *ClusterInfoResolver) ID() graphql.ID {
	return graphql.ID(c.clusterID.String())
}

func clusterInfoToResolver(cluster *cloudpb.ClusterInfo) (*ClusterInfoResolver, error) {
	clusterID, err := utils.UUIDFromProto(cluster.ID)
	if err != nil {
		return nil, err
	}

	var podStatuses []PodStatusResolver
	for _, podStatus := range cluster.ControlPlanePodStatuses {
		if podStatus == nil {
			continue
		}
		p := podStatusToResolver(*podStatus)
		podStatuses = append(podStatuses, p)
	}
	sort.SliceStable(podStatuses, func(i, j int) bool {
		return podStatuses[i].Name < podStatuses[j].Name
	})

	return &ClusterInfoResolver{
		clusterID:       clusterID,
		Status:          cluster.Status.String(),
		LastHeartbeatMs: float64(cluster.LastHeartbeatNs) / 1e6,
		VizierConfig: VizierConfigResolver{
			PassthroughEnabled: cluster.Config.PassthroughEnabled,
		},
		VizierVersion:           cluster.VizierVersion,
		ClusterVersion:          cluster.ClusterVersion,
		ClusterUID:              cluster.ClusterUID,
		ClusterName:             cluster.ClusterName,
		PrettyClusterName:       cluster.PrettyClusterName,
		ControlPlanePodStatuses: podStatuses,
		NumNodes:                cluster.NumNodes,
		NumInstrumentedNodes:    cluster.NumInstrumentedNodes,
	}, nil
}

// Clusters lists all of the clusters.
func (q *QueryResolver) Clusters(ctx context.Context) ([]*ClusterInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo
	resp, err := grpcAPI.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})
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
	grpcAPI := q.Env.VizierClusterInfo
	res, err := grpcAPI.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.ID)),
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

type clusterNameArgs struct {
	Name string
}

// ClusterByName resolves cluster information given a cluster name.
func (q *QueryResolver) ClusterByName(ctx context.Context, args *clusterNameArgs) (*ClusterInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo
	res, err := grpcAPI.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	if len(res.Clusters) == 0 {
		return nil, errors.New("org has no matching clusters")
	}

	for _, c := range res.Clusters {
		if c.ClusterName == args.Name {
			return clusterInfoToResolver(c)
		}
	}
	return nil, errors.New("Could not find cluster with name")
}

type editableVizierConfig struct {
	PassthroughEnabled *bool
}

type updateVizierConfigArgs struct {
	ClusterID    graphql.ID
	VizierConfig *editableVizierConfig
}

// UpdateVizierConfig updates the Vizier config of the input cluster
func (q *QueryResolver) UpdateVizierConfig(ctx context.Context, args *updateVizierConfigArgs) (*ClusterInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo

	clusterID := utils.ProtoFromUUIDStrOrNil(string(args.ClusterID))
	req := &cloudpb.UpdateClusterVizierConfigRequest{
		ID:           clusterID,
		ConfigUpdate: &cloudpb.VizierConfigUpdate{},
	}

	if args.VizierConfig.PassthroughEnabled != nil {
		req.ConfigUpdate.PassthroughEnabled = &types.BoolValue{Value: *args.VizierConfig.PassthroughEnabled}
	}

	_, err := grpcAPI.UpdateClusterVizierConfig(ctx, req)
	if err != nil {
		return nil, err
	}

	res, err := grpcAPI.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{
		ID: clusterID,
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

// ClusterConnectionInfoResolver is the resolver responsible for cluster connection info.
type ClusterConnectionInfoResolver struct {
	IPAddress string
	Token     string
}

// ClusterConnection resolves cluster connection information..
func (q *QueryResolver) ClusterConnection(ctx context.Context, args *clusterArgs) (*ClusterConnectionInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo

	clusterID := utils.ProtoFromUUIDStrOrNil(string(args.ID))
	info, err := grpcAPI.GetClusterConnectionInfo(ctx, &cloudpb.GetClusterConnectionInfoRequest{
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
