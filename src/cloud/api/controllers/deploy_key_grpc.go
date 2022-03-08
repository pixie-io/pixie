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

	"github.com/gogo/protobuf/types"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	apiUtils "px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// VizierDeploymentKeyServer is the server that implements the VizierDeploymentKeyManager gRPC service.
type VizierDeploymentKeyServer struct {
	VzDeploymentKey vzmgrpb.VZDeploymentKeyServiceClient
}

func deployKeyToCloudAPI(key *vzmgrpb.DeploymentKey) *cloudpb.DeploymentKey {
	return &cloudpb.DeploymentKey{
		ID:        key.ID,
		OrgID:     key.OrgID,
		UserID:    key.UserID,
		Key:       key.Key,
		CreatedAt: key.CreatedAt,
		Desc:      key.Desc,
	}
}

func deployKeyMetadataToCloudAPI(key *vzmgrpb.DeploymentKeyMetadata) *cloudpb.DeploymentKeyMetadata {
	return &cloudpb.DeploymentKeyMetadata{
		ID:        key.ID,
		OrgID:     key.OrgID,
		UserID:    key.UserID,
		CreatedAt: key.CreatedAt,
		Desc:      key.Desc,
	}
}

// Create creates a new deploy key in vzmgr.
func (v *VizierDeploymentKeyServer) Create(ctx context.Context, req *cloudpb.CreateDeploymentKeyRequest) (*cloudpb.DeploymentKey, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}
	orgID := apiUtils.ProtoFromUUIDStrOrNil(aCtx.Claims.GetUserClaims().OrgID)
	if orgID == nil {
		return nil, status.Error(codes.Internal, "error parsing org ID as UUID")
	}
	userID := apiUtils.ProtoFromUUIDStrOrNil(aCtx.Claims.GetUserClaims().UserID)
	if userID == nil {
		return nil, status.Error(codes.Internal, "error parsing user ID as UUID")
	}
	resp, err := v.VzDeploymentKey.Create(ctx, &vzmgrpb.CreateDeploymentKeyRequest{
		Desc:   req.Desc,
		OrgID:  orgID,
		UserID: userID,
	})
	if err != nil {
		return nil, err
	}
	return deployKeyToCloudAPI(resp), nil
}

// List lists all of the deploy keys in vzmgr.
func (v *VizierDeploymentKeyServer) List(ctx context.Context, req *cloudpb.ListDeploymentKeyRequest) (*cloudpb.ListDeploymentKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}
	orgID := apiUtils.ProtoFromUUIDStrOrNil(aCtx.Claims.GetUserClaims().OrgID)
	if orgID == nil {
		return nil, status.Error(codes.Internal, "error parsing org ID as UUID")
	}

	resp, err := v.VzDeploymentKey.List(ctx, &vzmgrpb.ListDeploymentKeyRequest{
		OrgID: orgID,
	})
	if err != nil {
		return nil, err
	}
	var keys []*cloudpb.DeploymentKeyMetadata
	for _, key := range resp.Keys {
		keys = append(keys, deployKeyMetadataToCloudAPI(key))
	}
	return &cloudpb.ListDeploymentKeyResponse{
		Keys: keys,
	}, nil
}

// Get fetches a specific deploy key in vzmgr.
func (v *VizierDeploymentKeyServer) Get(ctx context.Context, req *cloudpb.GetDeploymentKeyRequest) (*cloudpb.GetDeploymentKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgID := apiUtils.ProtoFromUUIDStrOrNil(aCtx.Claims.GetUserClaims().OrgID)
	if orgID == nil {
		return nil, status.Error(codes.Internal, "error parsing org ID as UUID")
	}

	resp, err := v.VzDeploymentKey.Get(ctx, &vzmgrpb.GetDeploymentKeyRequest{
		ID:    req.ID,
		OrgID: orgID,
	})
	if err != nil {
		return nil, err
	}
	return &cloudpb.GetDeploymentKeyResponse{
		Key: deployKeyToCloudAPI(resp.Key),
	}, nil
}

// Delete deletes a specific deploy key in vzmgr.
func (v *VizierDeploymentKeyServer) Delete(ctx context.Context, uuid *uuidpb.UUID) (*types.Empty, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgID := apiUtils.ProtoFromUUIDStrOrNil(aCtx.Claims.GetUserClaims().OrgID)
	if orgID == nil {
		return nil, status.Error(codes.Internal, "error parsing org ID as UUID")
	}

	if err != nil {
		return nil, err
	}
	return v.VzDeploymentKey.Delete(ctx, &vzmgrpb.DeleteDeploymentKeyRequest{
		OrgID: orgID,
		ID:    uuid,
	})
}

// LookupDeploymentKey gets the complete API key information using just the Key.
func (v *VizierDeploymentKeyServer) LookupDeploymentKey(ctx context.Context, req *cloudpb.LookupDeploymentKeyRequest) (*cloudpb.LookupDeploymentKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.VzDeploymentKey.LookupDeploymentKey(ctx, &vzmgrpb.LookupDeploymentKeyRequest{Key: req.Key})
	if err != nil {
		return nil, err
	}
	if utils.UUIDFromProtoOrNil(resp.Key.OrgID).String() != aCtx.Claims.GetUserClaims().OrgID {
		return nil, status.Error(codes.NotFound, "deployment key not found")
	}

	return &cloudpb.LookupDeploymentKeyResponse{Key: deployKeyToCloudAPI(resp.Key)}, nil
}
