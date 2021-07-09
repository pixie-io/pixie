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

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
)

// UserServiceServer is the server that implements the UserService gRPC service.
type UserServiceServer struct {
	ProfileServiceClient profilepb.ProfileServiceClient
}

// GetOrg will retrieve org based on uuid.
func (u *UserServiceServer) GetOrg(ctx context.Context, req *uuidpb.UUID) (*cloudpb.OrgInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := u.ProfileServiceClient.GetOrg(ctx, req)
	if err != nil {
		return nil, err
	}
	return &cloudpb.OrgInfo{
		ID:              resp.ID,
		OrgName:         resp.OrgName,
		DomainName:      resp.DomainName,
		EnableApprovals: resp.EnableApprovals,
	}, nil
}

// GetUser will retrieve user based on UUID.
func (u *UserServiceServer) GetUser(ctx context.Context, req *uuidpb.UUID) (*cloudpb.UserInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := u.ProfileServiceClient.GetUser(ctx, req)
	if err != nil {
		return nil, err
	}
	return &cloudpb.UserInfo{
		ID:             resp.ID,
		OrgID:          resp.OrgID,
		Username:       resp.Username,
		FirstName:      resp.FirstName,
		LastName:       resp.LastName,
		Email:          resp.Email,
		ProfilePicture: resp.ProfilePicture,
		IsApproved:     resp.IsApproved,
	}, nil
}

// GetUserSettings will retrieve settings based on UUID and keys.
func (u *UserServiceServer) GetUserSettings(ctx context.Context, req *cloudpb.GetUserSettingsRequest) (*cloudpb.GetUserSettingsResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.GetUserSettingsRequest{
		ID:   req.ID,
		Keys: req.Keys,
	}

	resp, err := u.ProfileServiceClient.GetUserSettings(ctx, in)
	if err != nil {
		return nil, err
	}

	settingsMap := make(map[string]string)
	for idx := range resp.Keys {
		settingsMap[resp.Keys[idx]] = resp.Values[idx]
	}

	return &cloudpb.GetUserSettingsResponse{
		SettingMap: settingsMap,
	}, nil
}

// UpdateUserSettings will update user settings.
func (u *UserServiceServer) UpdateUserSettings(ctx context.Context, req *cloudpb.UpdateUserSettingsRequest) (*cloudpb.UpdateUserSettingsResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	idx := 0
	keys := make([]string, len(req.SettingMap))
	values := make([]string, len(req.SettingMap))
	for k, v := range req.SettingMap {
		keys[idx] = k
		values[idx] = v
		idx++
	}

	in := &profilepb.UpdateUserSettingsRequest{
		ID:     req.ID,
		Keys:   keys,
		Values: values,
	}

	_, err = u.ProfileServiceClient.UpdateUserSettings(ctx, in)
	if err != nil {
		return nil, err
	}
	return &cloudpb.UpdateUserSettingsResponse{}, nil
}

// UpdateUser will update user information.
func (u *UserServiceServer) UpdateUser(ctx context.Context, req *cloudpb.UpdateUserRequest) (*cloudpb.UserInfo,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.UpdateUserRequest{
		ID:             req.ID,
		DisplayPicture: req.DisplayPicture,
		IsApproved:     req.IsApproved,
	}

	resp, err := u.ProfileServiceClient.UpdateUser(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.UserInfo{
		ID:             resp.ID,
		OrgID:          resp.OrgID,
		Username:       resp.Username,
		FirstName:      resp.FirstName,
		LastName:       resp.LastName,
		Email:          resp.Email,
		ProfilePicture: resp.ProfilePicture,
		IsApproved:     resp.IsApproved,
	}, nil
}

// GetUserAttributes will retrieve attributes given the user ID.
func (u *UserServiceServer) GetUserAttributes(ctx context.Context, req *cloudpb.GetUserAttributesRequest) (*cloudpb.GetUserAttributesResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.GetUserAttributesRequest{
		ID: req.ID,
	}

	resp, err := u.ProfileServiceClient.GetUserAttributes(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetUserAttributesResponse{
		TourSeen: resp.TourSeen,
	}, nil
}

// SetUserAttributes will update the attributes for the given user.
func (u *UserServiceServer) SetUserAttributes(ctx context.Context, req *cloudpb.SetUserAttributesRequest) (*cloudpb.SetUserAttributesResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.SetUserAttributesRequest{
		ID:       req.ID,
		TourSeen: req.TourSeen,
	}

	_, err = u.ProfileServiceClient.SetUserAttributes(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.SetUserAttributesResponse{}, nil
}
