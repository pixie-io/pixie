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

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/auth/authpb"
)

// APIKeyServer is the server that implements the APIKeyManager gRPC service.
type APIKeyServer struct {
	APIKeyClient authpb.APIKeyServiceClient
}

func apiKeyToCloudAPI(key *authpb.APIKey) *cloudpb.APIKey {
	return &cloudpb.APIKey{
		ID:        key.ID,
		OrgID:     key.OrgID,
		UserID:    key.UserID,
		Key:       key.Key,
		CreatedAt: key.CreatedAt,
		Desc:      key.Desc,
	}
}

func apiKeyMetadataToCloudAPI(key *authpb.APIKeyMetadata) *cloudpb.APIKeyMetadata {
	return &cloudpb.APIKeyMetadata{
		ID:        key.ID,
		OrgID:     key.OrgID,
		UserID:    key.UserID,
		CreatedAt: key.CreatedAt,
		Desc:      key.Desc,
	}
}

// Create creates a new API key.
func (v *APIKeyServer) Create(ctx context.Context, req *cloudpb.CreateAPIKeyRequest) (*cloudpb.APIKey, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.APIKeyClient.Create(ctx, &authpb.CreateAPIKeyRequest{Desc: req.Desc})
	if err != nil {
		return nil, err
	}
	return apiKeyToCloudAPI(resp), nil
}

// List lists all of the API keys in vzmgr.
func (v *APIKeyServer) List(ctx context.Context, req *cloudpb.ListAPIKeyRequest) (*cloudpb.ListAPIKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.APIKeyClient.List(ctx, &authpb.ListAPIKeyRequest{})
	if err != nil {
		return nil, err
	}
	var keys []*cloudpb.APIKeyMetadata
	for _, key := range resp.Keys {
		keys = append(keys, apiKeyMetadataToCloudAPI(key))
	}
	return &cloudpb.ListAPIKeyResponse{
		Keys: keys,
	}, nil
}

// Get fetches a specific API key.
func (v *APIKeyServer) Get(ctx context.Context, req *cloudpb.GetAPIKeyRequest) (*cloudpb.GetAPIKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.APIKeyClient.Get(ctx, &authpb.GetAPIKeyRequest{
		ID: req.ID,
	})
	if err != nil {
		return nil, err
	}
	return &cloudpb.GetAPIKeyResponse{
		Key: apiKeyToCloudAPI(resp.Key),
	}, nil
}

// Delete deletes a specific API key.
func (v *APIKeyServer) Delete(ctx context.Context, uuid *uuidpb.UUID) (*types.Empty, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	return v.APIKeyClient.Delete(ctx, uuid)
}

// LookupAPIKey gets the complete API key information using just the Key.
func (v *APIKeyServer) LookupAPIKey(ctx context.Context, req *cloudpb.LookupAPIKeyRequest) (*cloudpb.LookupAPIKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	resp, err := v.APIKeyClient.LookupAPIKey(ctx, &authpb.LookupAPIKeyRequest{Key: req.Key})
	if err != nil {
		return nil, err
	}
	return &cloudpb.LookupAPIKeyResponse{Key: apiKeyToCloudAPI(resp.Key)}, nil
}
