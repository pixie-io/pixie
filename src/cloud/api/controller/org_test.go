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

package controller_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controller"
	"px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/utils"
)

func TestOrganizationServiceServer_InviteUser(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx
			mockReq := &authpb.InviteUserRequest{
				OrgID:     utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Email:     "bobloblaw@lawblog.law",
				FirstName: "bob",
				LastName:  "loblaw",
			}

			mockClients.MockAuth.EXPECT().InviteUser(gomock.Any(), mockReq).
				Return(&authpb.InviteUserResponse{
					InviteLink: "withpixie.ai/invite&id=abcd",
				}, nil)

			os := &controller.OrganizationServiceServer{mockClients.MockProfile, mockClients.MockAuth, mockClients.MockOrg}

			resp, err := os.InviteUser(ctx, &cloudpb.InviteUserRequest{
				Email:     "bobloblaw@lawblog.law",
				FirstName: "bob",
				LastName:  "loblaw",
			})

			require.NoError(t, err)
			assert.Equal(t, mockReq.Email, resp.Email)
			assert.Equal(t, "withpixie.ai/invite&id=abcd", resp.InviteLink)
		})
	}
}

func TestOrganizationServiceServer_AddOrgIDEConfig(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateAPIUserTestContext()

	mockClients.MockOrg.EXPECT().AddOrgIDEConfig(gomock.Any(), &profilepb.AddOrgIDEConfigRequest{
		OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Config: &profilepb.IDEConfig{
			IDEName: "test",
			Path:    "test://{{symbol}}",
		},
	}).Return(&profilepb.AddOrgIDEConfigResponse{
		Config: &profilepb.IDEConfig{
			IDEName: "test",
			Path:    "test://{{symbol}}",
		},
	}, nil)

	os := &controller.OrganizationServiceServer{mockClients.MockProfile, mockClients.MockAuth, mockClients.MockOrg}

	resp, err := os.AddOrgIDEConfig(ctx, &cloudpb.AddOrgIDEConfigRequest{
		OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Config: &cloudpb.IDEConfig{
			IDEName: "test",
			Path:    "test://{{symbol}}",
		},
	})

	require.NoError(t, err)
	assert.Equal(t, "test", resp.Config.IDEName)
	assert.Equal(t, "test://{{symbol}}", resp.Config.Path)
}

func TestOrganizationServiceServer_DeleteOrgIDEConfig(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateAPIUserTestContext()

	mockClients.MockOrg.EXPECT().DeleteOrgIDEConfig(gomock.Any(), &profilepb.DeleteOrgIDEConfigRequest{
		OrgID:   utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		IDEName: "test",
	}).Return(&profilepb.DeleteOrgIDEConfigResponse{}, nil)

	os := &controller.OrganizationServiceServer{mockClients.MockProfile, mockClients.MockAuth, mockClients.MockOrg}

	resp, err := os.DeleteOrgIDEConfig(ctx, &cloudpb.DeleteOrgIDEConfigRequest{
		OrgID:   utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		IDEName: "test",
	})

	require.NoError(t, err)
	assert.NotNil(t, resp)
}

func TestOrganizationServiceServer_GetOrgIDEConfigs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateAPIUserTestContext()

	mockClients.MockOrg.EXPECT().GetOrgIDEConfigs(gomock.Any(), &profilepb.GetOrgIDEConfigsRequest{
		OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
	}).Return(&profilepb.GetOrgIDEConfigsResponse{
		Configs: []*profilepb.IDEConfig{
			&profilepb.IDEConfig{
				IDEName: "test",
				Path:    "test://{{symbol}}",
			},
			&profilepb.IDEConfig{
				IDEName: "another-test",
				Path:    "sublime://{{symbol}}",
			},
		},
	}, nil)

	os := &controller.OrganizationServiceServer{mockClients.MockProfile, mockClients.MockAuth, mockClients.MockOrg}

	resp, err := os.GetOrgIDEConfigs(ctx, &cloudpb.GetOrgIDEConfigsRequest{
		OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
	})

	require.NoError(t, err)
	assert.Equal(t, []*cloudpb.IDEConfig{
		&cloudpb.IDEConfig{
			IDEName: "test",
			Path:    "test://{{symbol}}",
		},
		&cloudpb.IDEConfig{
			IDEName: "another-test",
			Path:    "sublime://{{symbol}}",
		},
	}, resp.Configs)
}
