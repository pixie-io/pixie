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

package pxapi

import (
	"context"

	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/proto/cloudpb"
)

// VizierStatus stores the enumeration of all vizier statuses.
type VizierStatus string

// Vizier Statuses.
const (
	VizierStatusUnknown      VizierStatus = "Unknown"
	VizierStatusHealthy      VizierStatus = "Healthy"
	VizierStatusUnhealthy    VizierStatus = "Unhealthy"
	VizierStatusDisconnected VizierStatus = "Disconnected"
)

// VizierInfo has information of a single Vizier.
type VizierInfo struct {
	// Name of the vizier.
	Name string
	// ID of the Vizier (uuid as a string).
	ID string
	// Status of the Vizier.
	Status VizierStatus
	// Version of the installed vizier.
	Version string
	// DirectAccess says the cluster has direct access mode enabled. This means the data transfer will bypass the cloud.
	DirectAccess bool
}

func clusterStatusToVizierStatus(status cloudpb.ClusterStatus) VizierStatus {
	switch status {
	case cloudpb.CS_HEALTHY:
		return VizierStatusHealthy
	case cloudpb.CS_UNHEALTHY:
		return VizierStatusUnhealthy
	case cloudpb.CS_DISCONNECTED:
		return VizierStatusDisconnected
	default:
		return VizierStatusUnknown
	}
}

// ListViziers gets a list of Viziers registered with Pixie.
func (c *Client) ListViziers(ctx context.Context) ([]*VizierInfo, error) {
	req := &cloudpb.GetClusterInfoRequest{}
	res, err := c.cmClient.GetClusterInfo(c.cloudCtxWithMD(ctx), req)
	if err != nil {
		return nil, err
	}

	viziers := make([]*VizierInfo, 0)
	for _, v := range res.Clusters {
		viziers = append(viziers, &VizierInfo{
			Name:         v.ClusterName,
			ID:           ProtoToUUIDStr(v.ID),
			Version:      v.VizierVersion,
			Status:       clusterStatusToVizierStatus(v.Status),
			DirectAccess: !v.Config.PassthroughEnabled,
		})
	}

	return viziers, nil
}

// GetVizierInfo gets info about the given clusterID.
func (c *Client) GetVizierInfo(ctx context.Context, clusterID string) (*VizierInfo, error) {
	req := &cloudpb.GetClusterInfoRequest{
		ID: ProtoFromUUIDStrOrNil(clusterID),
	}
	res, err := c.cmClient.GetClusterInfo(c.cloudCtxWithMD(ctx), req)
	if err != nil {
		return nil, err
	}

	if len(res.Clusters) == 0 {
		return nil, errdefs.ErrClusterNotFound
	}

	v := res.Clusters[0]

	return &VizierInfo{
		Name:         v.ClusterName,
		ID:           ProtoToUUIDStr(v.ID),
		Version:      v.VizierVersion,
		Status:       clusterStatusToVizierStatus(v.Status),
		DirectAccess: !v.Config.PassthroughEnabled,
	}, nil
}

// getConnectionInfo gets the connection info for a cluster using direct mode.
func (c *Client) getConnectionInfo(ctx context.Context, clusterID string) (*cloudpb.GetClusterConnectionInfoResponse, error) {
	req := &cloudpb.GetClusterConnectionInfoRequest{
		ID: ProtoFromUUIDStrOrNil(clusterID),
	}
	return c.cmClient.GetClusterConnectionInfo(c.cloudCtxWithMD(ctx), req)
}

// CreateDeployKey creates a new deploy key, with an optional description.
func (c *Client) CreateDeployKey(ctx context.Context, desc string) (*cloudpb.DeploymentKey, error) {
	keyMgr := cloudpb.NewVizierDeploymentKeyManagerClient(c.grpcConn)
	req := &cloudpb.CreateDeploymentKeyRequest{
		Desc: desc,
	}
	dk, err := keyMgr.Create(c.cloudCtxWithMD(ctx), req)
	if err != nil {
		return nil, err
	}
	return dk, nil
}

// CreateAPIKey creates and API key with the passed in description.
func (c *Client) CreateAPIKey(ctx context.Context, desc string) (*cloudpb.APIKey, error) {
	req := &cloudpb.CreateAPIKeyRequest{
		Desc: desc,
	}

	apiKeyMgr := cloudpb.NewAPIKeyManagerClient(c.grpcConn)
	resp, err := apiKeyMgr.Create(c.cloudCtxWithMD(ctx), req)
	if err != nil {
		return nil, err
	}

	return resp, nil
}

// DeleteAPIKey deletes an API key by ID.
func (c *Client) DeleteAPIKey(ctx context.Context, id string) error {
	req := ProtoFromUUIDStrOrNil(id)
	apiKeyMgr := cloudpb.NewAPIKeyManagerClient(c.grpcConn)
	_, err := apiKeyMgr.Delete(c.cloudCtxWithMD(ctx), req)
	return err
}
