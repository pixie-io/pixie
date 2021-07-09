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

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// ProfileServer provides info about users and orgs.
type ProfileServer struct {
	ProfileServiceClient profilepb.ProfileServiceClient
}

// GetOrgInfo gets the org info for a given org ID.
func (p *ProfileServer) GetOrgInfo(ctx context.Context, req *uuidpb.UUID) (*cloudpb.OrgInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	claimsOrgID := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.UUIDFromProtoOrNil(req)
	if claimsOrgID != orgID.String() {
		return nil, status.Error(codes.Unauthenticated, "Unable to fetch org info")
	}

	resp, err := p.ProfileServiceClient.GetOrg(ctx, req)
	if err != nil {
		return nil, err
	}

	return &cloudpb.OrgInfo{
		ID:      resp.ID,
		OrgName: resp.OrgName,
	}, nil
}
