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
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controller"
	"px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/utils"
)

func TestOrganizationServiceServer_InviteUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()
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

	os := &controller.OrganizationServiceServer{mockClients.MockProfile, mockClients.MockAuth}

	resp, err := os.InviteUser(ctx, &cloudpb.InviteUserRequest{
		Email:     "bobloblaw@lawblog.law",
		FirstName: "bob",
		LastName:  "loblaw",
	})

	require.NoError(t, err)
	assert.Equal(t, mockReq.Email, resp.Email)
	assert.Equal(t, "withpixie.ai/invite&id=abcd", resp.InviteLink)
}
