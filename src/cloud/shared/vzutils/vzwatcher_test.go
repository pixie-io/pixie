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

package vzutils_test

import (
	"errors"
	"sync"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/shared/messages"
	"px.dev/pixie/src/cloud/shared/messagespb"
	"px.dev/pixie/src/cloud/shared/vzutils"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	mock_vzmgrpb "px.dev/pixie/src/cloud/vzmgr/vzmgrpb/mock"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestVzWatcher(t *testing.T) {
	tests := []struct {
		name        string
		expectError bool
	}{
		{
			name:        "no error",
			expectError: false,
		},
		{
			name:        "error",
			expectError: true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			viper.Set("jwt_signing_key", "jwtkey")

			ctrl := gomock.NewController(t)
			mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)

			existingVzID := uuid.Must(uuid.NewV4())
			existingOrgID := uuid.Must(uuid.NewV4())
			existingK8sUID := "testUID"

			mockVZMgr.
				EXPECT().
				GetViziersByShard(gomock.Any(), &vzmgrpb.GetViziersByShardRequest{
					FromShardID: "00",
					ToShardID:   "bb",
				}).
				Return(&vzmgrpb.GetViziersByShardResponse{
					Viziers: []*vzmgrpb.GetViziersByShardResponse_VizierInfo{
						{
							VizierID: utils.ProtoFromUUID(existingVzID),
							OrgID:    utils.ProtoFromUUID(existingOrgID),
							K8sUID:   existingK8sUID,
						},
					},
				}, nil)

			nc, natsCleanup := testingutils.MustStartTestNATS(t)
			defer natsCleanup()

			w, err := vzutils.NewWatcher(nc, mockVZMgr, "00", "bb")
			require.NoError(t, err)

			var wg sync.WaitGroup
			wg.Add(2)

			if test.expectError {
				wg.Add(1)
			}

			defer wg.Wait()

			newVzID := uuid.Must(uuid.NewV4())
			newOrgID := uuid.Must(uuid.NewV4())
			newK8sUID := "testUID"

			w.RegisterErrorHandler(func(id uuid.UUID, orgID uuid.UUID, uid string, err error) {
				defer wg.Done()
				assert.Equal(t, existingVzID, id)
				assert.Equal(t, existingOrgID, orgID)
				assert.Equal(t, existingK8sUID, uid)
			})

			err = w.RegisterVizierHandler(func(id uuid.UUID, orgID uuid.UUID, uid string) error {
				defer wg.Done()

				switch id {
				case existingVzID:
					assert.Equal(t, existingOrgID, orgID)
					assert.Equal(t, existingK8sUID, uid)
					if test.expectError {
						return errors.New("Some error")
					}
				case newVzID:
					assert.Equal(t, newOrgID, orgID)
					assert.Equal(t, newK8sUID, uid)
				default:
					t.Fatal("Called Vizier handler with unexpected vizier")
				}
				return nil
			})
			require.NoError(t, err)

			msg := &messagespb.VizierConnected{
				VizierID: utils.ProtoFromUUID(newVzID),
				OrgID:    utils.ProtoFromUUID(newOrgID),
				K8sUID:   newK8sUID,
			}
			b, err := msg.Marshal()
			require.NoError(t, err)
			err = nc.Publish(messages.VizierConnectedChannel, b)
			require.NoError(t, err)
		})
	}
}
