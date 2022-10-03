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
	"px.dev/pixie/src/api/go/pxapi/utils"
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
	VizierStatusDegraded     VizierStatus = "Degraded"
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
}

func clusterStatusToVizierStatus(status cloudpb.ClusterStatus) VizierStatus {
	switch status {
	case cloudpb.CS_HEALTHY:
		return VizierStatusHealthy
	case cloudpb.CS_UNHEALTHY:
		return VizierStatusUnhealthy
	case cloudpb.CS_DISCONNECTED:
		return VizierStatusDisconnected
	case cloudpb.CS_DEGRADED:
		return VizierStatusDegraded
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
			Name:    v.ClusterName,
			ID:      utils.ProtoToUUIDStr(v.ID),
			Version: v.VizierVersion,
			Status:  clusterStatusToVizierStatus(v.Status),
		})
	}

	return viziers, nil
}

// GetVizierInfo gets info about the given clusterID.
func (c *Client) GetVizierInfo(ctx context.Context, clusterID string) (*VizierInfo, error) {
	req := &cloudpb.GetClusterInfoRequest{
		ID: utils.ProtoFromUUIDStrOrNil(clusterID),
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
		Name:    v.ClusterName,
		ID:      utils.ProtoToUUIDStr(v.ID),
		Version: v.VizierVersion,
		Status:  clusterStatusToVizierStatus(v.Status),
	}, nil
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
	req := utils.ProtoFromUUIDStrOrNil(id)
	apiKeyMgr := cloudpb.NewAPIKeyManagerClient(c.grpcConn)
	_, err := apiKeyMgr.Delete(c.cloudCtxWithMD(ctx), req)
	return err
}

// GetLatestVizierVersion returns the latest version of vizier available.
func (c *Client) GetLatestVizierVersion(ctx context.Context) (string, error) {
	return c.getLatestArtifact(ctx, "vizier", cloudpb.AT_CONTAINER_SET_YAMLS)
}

// GetLatestOperatorVersion returns the latest version of the operator available.
func (c *Client) GetLatestOperatorVersion(ctx context.Context) (string, error) {
	return c.getLatestArtifact(ctx, "operator", cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS)
}

// Helper function to fetch artifact versions.
func (c *Client) getLatestArtifact(ctx context.Context, an string, at cloudpb.ArtifactType) (string, error) {
	ac := cloudpb.NewArtifactTrackerClient(c.grpcConn)
	req := &cloudpb.GetArtifactListRequest{
		ArtifactName: an,
		ArtifactType: at,
		Limit:        1,
	}
	resp, err := ac.GetArtifactList(c.cloudCtxWithMD(ctx), req)
	if err != nil {
		return "", err
	}
	if len(resp.Artifact) != 1 {
		return "", errdefs.ErrMissingArtifact
	}
	return resp.Artifact[0].VersionStr, nil
}
