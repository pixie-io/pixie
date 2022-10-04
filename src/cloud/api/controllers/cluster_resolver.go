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

package controllers

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
	Name         string
	CreatedAtMs  float64
	State        string
	Message      string
	Reason       string
	RestartCount int32
}

func containerStatusToResolver(containerStatus cloudpb.ContainerStatus) ContainerStatusResolver {
	resolver := ContainerStatusResolver{
		Name:         containerStatus.Name,
		State:        containerStatus.State.String(),
		Message:      containerStatus.Message,
		Reason:       containerStatus.Reason,
		RestartCount: int32(containerStatus.RestartCount),
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
	Name         string
	CreatedAtMs  float64
	Status       string
	Message      string
	Reason       string
	Containers   []ContainerStatusResolver
	Events       []K8sEventResolver
	RestartCount int32
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
		Name:         podStatus.Name,
		Status:       podStatus.Status.String(),
		Message:      podStatus.StatusMessage,
		Reason:       podStatus.Reason,
		Containers:   containers,
		Events:       events,
		RestartCount: int32(podStatus.RestartCount),
	}

	if podStatus.CreatedAt != nil {
		resolver.CreatedAtMs = timestampProtoToMillis(podStatus.CreatedAt)
	}
	return resolver
}

// ClusterInfoResolver is the resolver responsible for cluster info.
type ClusterInfoResolver struct {
	clusterID                     uuid.UUID
	Status                        string
	LastHeartbeatMs               float64
	VizierVersion                 string
	OperatorVersion               string
	ClusterVersion                string
	ClusterUID                    string
	ClusterName                   string
	PrettyClusterName             string
	StatusMessage                 string
	ControlPlanePodStatuses       []PodStatusResolver
	UnhealthyDataPlanePodStatuses []PodStatusResolver
	NumNodes                      int32
	NumInstrumentedNodes          int32
	PreviousStatus                *string
	PreviousStatusTimeMs          *float64
}

// ID returns cluster ID.
func (c *ClusterInfoResolver) ID() graphql.ID {
	return graphql.ID(c.clusterID.String())
}

// Helper to map proto statuses to GQL pod statuses.
func mapPodStatusArray(inputStatuses map[string]*cloudpb.PodStatus) []PodStatusResolver {
	var podStatuses []PodStatusResolver
	for _, podStatus := range inputStatuses {
		if podStatus == nil {
			continue
		}
		p := podStatusToResolver(*podStatus)
		podStatuses = append(podStatuses, p)
	}
	sort.SliceStable(podStatuses, func(i, j int) bool {
		return podStatuses[i].Name < podStatuses[j].Name
	})
	return podStatuses
}

func clusterInfoToResolver(cluster *cloudpb.ClusterInfo) (*ClusterInfoResolver, error) {
	clusterID, err := utils.UUIDFromProto(cluster.ID)
	if err != nil {
		return nil, err
	}

	resolver := &ClusterInfoResolver{
		clusterID:                     clusterID,
		Status:                        cluster.Status.String(),
		LastHeartbeatMs:               float64(cluster.LastHeartbeatNs) / 1e6,
		VizierVersion:                 cluster.VizierVersion,
		OperatorVersion:               cluster.OperatorVersion,
		ClusterVersion:                cluster.ClusterVersion,
		ClusterUID:                    cluster.ClusterUID,
		ClusterName:                   cluster.ClusterName,
		PrettyClusterName:             cluster.PrettyClusterName,
		StatusMessage:                 cluster.StatusMessage,
		ControlPlanePodStatuses:       mapPodStatusArray(cluster.ControlPlanePodStatuses),
		UnhealthyDataPlanePodStatuses: mapPodStatusArray(cluster.UnhealthyDataPlanePodStatuses),
		NumNodes:                      cluster.NumNodes,
		NumInstrumentedNodes:          cluster.NumInstrumentedNodes,
	}

	if cluster.PreviousStatusTime != nil {
		status := cluster.PreviousStatus.String()
		prevTime := timestampProtoToMillis(cluster.PreviousStatusTime)
		resolver.PreviousStatusTimeMs = &prevTime
		resolver.PreviousStatus = &status
	}

	return resolver, nil
}

// Clusters lists all of the clusters.
func (q *QueryResolver) Clusters(ctx context.Context) ([]*ClusterInfoResolver, error) {
	grpcAPI := q.Env.VizierClusterInfo
	resp, err := grpcAPI.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})
	if err != nil {
		return nil, rpcErrorHelper(err)
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
		return nil, rpcErrorHelper(err)
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
		return nil, rpcErrorHelper(err)
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
