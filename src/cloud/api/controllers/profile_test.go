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

package controllers_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/utils"
)

func TestProfileServer_GetOrgInfo(t *testing.T) {
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
			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockOrg.EXPECT().GetOrg(gomock.Any(), orgID).
				Return(&profilepb.OrgInfo{
					OrgName: "someOrg",
					ID:      orgID,
				}, nil)

			profileServer := &controllers.ProfileServer{mockClients.MockOrg}

			resp, err := profileServer.GetOrgInfo(ctx, orgID)

			require.NoError(t, err)
			assert.Equal(t, "someOrg", resp.OrgName)
			assert.Equal(t, orgID, resp.ID)
		})
	}
}
