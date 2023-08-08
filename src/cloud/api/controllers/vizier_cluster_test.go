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

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/utils"
)

func TestVizierClusterInfo_GetClusterConnectionInfo(t *testing.T) {
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
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockVzMgr.EXPECT().GetVizierConnectionInfo(gomock.Any(), clusterID).Return(&cvmsgspb.VizierConnectionInfo{
				Token: "hello",
			}, nil)

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr: mockClients.MockVzMgr,
			}

			resp, err := vzClusterInfoServer.GetClusterConnectionInfo(ctx, &cloudpb.GetClusterConnectionInfoRequest{ID: clusterID})
			require.NoError(t, err)
			assert.Equal(t, "hello", resp.Token)
		})
	}
}

func TestVizierClusterInfo_GetClusterInfo(t *testing.T) {
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
			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
			assert.NotNil(t, clusterID)

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), orgID).Return(&vzmgrpb.GetViziersByOrgResponse{
				VizierIDs: []*uuidpb.UUID{clusterID},
			}, nil)

			mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
				VizierIDs: []*uuidpb.UUID{clusterID},
			}).Return(&vzmgrpb.GetVizierInfosResponse{
				VizierInfos: []*cvmsgspb.VizierInfo{{
					VizierID:        clusterID,
					Status:          cvmsgspb.VZ_ST_HEALTHY,
					StatusMessage:   "Everything is running",
					LastHeartbeatNs: int64(1305646598000000000),
					Config:          &cvmsgspb.VizierConfig{},
					VizierVersion:   "1.2.3",
					OperatorVersion: "0.0.30",
					ClusterUID:      "a UID",
					ClusterName:     "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar-3",
					ClusterVersion:  "5.6.7",
					ControlPlanePodStatuses: map[string]*cvmsgspb.PodStatus{
						"vizier-proxy": {
							Name:   "vizier-proxy",
							Status: metadatapb.RUNNING,
							Containers: []*cvmsgspb.ContainerStatus{
								{
									Name:      "my-proxy-container",
									State:     metadatapb.CONTAINER_STATE_RUNNING,
									Message:   "container message",
									Reason:    "container reason",
									CreatedAt: &types.Timestamp{Seconds: 1561230620},
								},
							},
							Events: []*cvmsgspb.K8SEvent{
								{
									Message:   "this is a test event",
									FirstTime: &types.Timestamp{Seconds: 1561230620},
									LastTime:  &types.Timestamp{Seconds: 1561230625},
								},
							},
							StatusMessage: "pod message",
							Reason:        "pod reason",
							CreatedAt:     &types.Timestamp{Seconds: 1561230621},
						},
						"vizier-query-broker": {
							Name:   "vizier-query-broker",
							Status: metadatapb.RUNNING,
						},
					},
					NumNodes:             5,
					NumInstrumentedNodes: 3,
				}},
			}, nil)

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr: mockClients.MockVzMgr,
			}

			resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})

			expectedPodStatuses := map[string]*cloudpb.PodStatus{
				"vizier-proxy": {
					Name:   "vizier-proxy",
					Status: cloudpb.RUNNING,
					Containers: []*cloudpb.ContainerStatus{
						{
							Name:      "my-proxy-container",
							State:     cloudpb.CONTAINER_STATE_RUNNING,
							Message:   "container message",
							Reason:    "container reason",
							CreatedAt: &types.Timestamp{Seconds: 1561230620},
						},
					},
					Events: []*cloudpb.K8SEvent{
						{
							Message:   "this is a test event",
							FirstTime: &types.Timestamp{Seconds: 1561230620},
							LastTime:  &types.Timestamp{Seconds: 1561230625},
						},
					},
					StatusMessage: "pod message",
					Reason:        "pod reason",
					CreatedAt:     &types.Timestamp{Seconds: 1561230621},
				},
				"vizier-query-broker": {
					Name:      "vizier-query-broker",
					Status:    cloudpb.RUNNING,
					CreatedAt: nil,
				},
			}

			require.NoError(t, err)
			assert.Equal(t, 1, len(resp.Clusters))
			cluster := resp.Clusters[0]
			assert.Equal(t, cluster.ID, clusterID)
			assert.Equal(t, cluster.Status, cloudpb.CS_HEALTHY)
			assert.Equal(t, cluster.LastHeartbeatNs, int64(1305646598000000000))
			assert.Equal(t, "1.2.3", cluster.VizierVersion)
			assert.Equal(t, "0.0.30", cluster.OperatorVersion)
			assert.Equal(t, "a UID", cluster.ClusterUID)
			assert.Equal(t, "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar-3", cluster.ClusterName)
			assert.Equal(t, "gke:dev-cluster-zasgar-3", cluster.PrettyClusterName)
			assert.Equal(t, "5.6.7", cluster.ClusterVersion)
			assert.Equal(t, "Everything is running", cluster.StatusMessage)
			assert.Equal(t, expectedPodStatuses, cluster.ControlPlanePodStatuses)
			assert.Equal(t, int32(5), cluster.NumNodes)
			assert.Equal(t, int32(3), cluster.NumInstrumentedNodes)
		})
	}
}

func TestVizierClusterInfo_GetClusterInfoDuplicates(t *testing.T) {
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
			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
			assert.NotNil(t, clusterID)
			clusterID2 := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9")

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), orgID).Return(&vzmgrpb.GetViziersByOrgResponse{
				VizierIDs: []*uuidpb.UUID{clusterID, clusterID2},
			}, nil)

			mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
				VizierIDs: []*uuidpb.UUID{clusterID, clusterID2},
			}).Return(&vzmgrpb.GetVizierInfosResponse{
				VizierInfos: []*cvmsgspb.VizierInfo{{
					VizierID:             clusterID,
					Status:               cvmsgspb.VZ_ST_HEALTHY,
					LastHeartbeatNs:      int64(1305646598000000000),
					Config:               &cvmsgspb.VizierConfig{},
					VizierVersion:        "1.2.3",
					ClusterUID:           "a UID",
					ClusterName:          "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar",
					ClusterVersion:       "5.6.7",
					NumNodes:             5,
					NumInstrumentedNodes: 3,
				},
					{
						VizierID:             clusterID,
						Status:               cvmsgspb.VZ_ST_HEALTHY,
						LastHeartbeatNs:      int64(1305646598000000000),
						Config:               &cvmsgspb.VizierConfig{},
						VizierVersion:        "1.2.3",
						ClusterUID:           "a UID2",
						ClusterName:          "gke_pl-pixies_us-west1-a_dev-cluster-zasgar",
						ClusterVersion:       "5.6.7",
						NumNodes:             5,
						NumInstrumentedNodes: 3,
					},
				},
			}, nil)

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr: mockClients.MockVzMgr,
			}

			resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})

			require.NoError(t, err)
			assert.Equal(t, 2, len(resp.Clusters))
			assert.Equal(t, "gke:dev-cluster-zasgar (pl-dev-infra)", resp.Clusters[0].PrettyClusterName)
			assert.Equal(t, "gke:dev-cluster-zasgar (pl-pixies)", resp.Clusters[1].PrettyClusterName)
		})
	}
}

func TestVizierClusterInfo_GetClusterInfo_Homoglyphs(t *testing.T) {
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
			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
			assert.NotNil(t, clusterID)
			clusterID2 := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9")

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), orgID).Return(&vzmgrpb.GetViziersByOrgResponse{
				VizierIDs: []*uuidpb.UUID{clusterID, clusterID2},
			}, nil)

			mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
				VizierIDs: []*uuidpb.UUID{clusterID, clusterID2},
			}).Return(&vzmgrpb.GetVizierInfosResponse{
				VizierInfos: []*cvmsgspb.VizierInfo{{
					VizierID:             clusterID,
					Status:               cvmsgspb.VZ_ST_HEALTHY,
					LastHeartbeatNs:      int64(1305646598000000000),
					Config:               &cvmsgspb.VizierConfig{},
					VizierVersion:        "1.2.3",
					ClusterUID:           "a UID",
					ClusterName:          "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar",
					ClusterVersion:       "5.6.7",
					NumNodes:             5,
					NumInstrumentedNodes: 3,
				},
					{
						VizierID:             clusterID,
						Status:               cvmsgspb.VZ_ST_HEALTHY,
						LastHeartbeatNs:      int64(1305646598000000000),
						Config:               &cvmsgspb.VizierConfig{},
						VizierVersion:        "1.2.3",
						ClusterUID:           "a UID2",
						ClusterName:          "gke_pl-dev-infra_us-west1-a_dev-cluster-zаsɡar",
						ClusterVersion:       "5.6.7",
						NumNodes:             5,
						NumInstrumentedNodes: 3,
					},
				},
			}, nil)

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr: mockClients.MockVzMgr,
			}

			resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})

			require.NoError(t, err)
			assert.Equal(t, 2, len(resp.Clusters))
			assert.Equal(t, "gke:dev-cluster-zasgar", resp.Clusters[0].PrettyClusterName)
			assert.Equal(t, "xn--gke:dev-cluster-zsar-eji892h", resp.Clusters[1].PrettyClusterName)
		})
	}
}

func TestVizierClusterInfo_GetClusterInfoWithID(t *testing.T) {
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
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
			assert.NotNil(t, clusterID)

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
				VizierIDs: []*uuidpb.UUID{clusterID},
			}).Return(&vzmgrpb.GetVizierInfosResponse{
				VizierInfos: []*cvmsgspb.VizierInfo{{
					VizierID:        clusterID,
					Status:          cvmsgspb.VZ_ST_HEALTHY,
					LastHeartbeatNs: int64(1305646598000000000),
					Config:          &cvmsgspb.VizierConfig{},
					VizierVersion:   "1.2.3",
					ClusterUID:      "a UID",
					ClusterName:     "some cluster",
					ClusterVersion:  "5.6.7",
				},
				},
			}, nil)

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr: mockClients.MockVzMgr,
			}

			resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{
				ID: clusterID,
			})

			require.NoError(t, err)
			assert.Equal(t, 1, len(resp.Clusters))
			cluster := resp.Clusters[0]
			assert.Equal(t, cluster.ID, clusterID)
			assert.Equal(t, cluster.Status, cloudpb.CS_HEALTHY)
			assert.Equal(t, cluster.LastHeartbeatNs, int64(1305646598000000000))
		})
	}
}

func TestVizierClusterInfo_UpdateClusterVizierConfig(t *testing.T) {
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
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
			assert.NotNil(t, clusterID)

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr: mockClients.MockVzMgr,
			}

			resp, err := vzClusterInfoServer.UpdateClusterVizierConfig(ctx, &cloudpb.UpdateClusterVizierConfigRequest{
				ID:           clusterID,
				ConfigUpdate: &cloudpb.VizierConfigUpdate{},
			})

			require.NoError(t, err)
			assert.NotNil(t, resp)
		})
	}
}

func TestVizierClusterInfo_UpdateOrInstallCluster(t *testing.T) {
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
			clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
			assert.NotNil(t, clusterID)

			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			updateReq := &cvmsgspb.UpdateOrInstallVizierRequest{
				VizierID: clusterID,
				Version:  "0.1.30",
			}

			mockClients.MockVzMgr.EXPECT().UpdateOrInstallVizier(gomock.Any(), updateReq).Return(&cvmsgspb.UpdateOrInstallVizierResponse{UpdateStarted: true}, nil)

			mockClients.MockArtifact.EXPECT().
				GetDownloadLink(gomock.Any(), &artifacttrackerpb.GetDownloadLinkRequest{
					ArtifactName: "vizier",
					VersionStr:   "0.1.30",
					ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
				}).
				Return(nil, nil)

			vzClusterInfoServer := &controllers.VizierClusterInfo{
				VzMgr:                 mockClients.MockVzMgr,
				ArtifactTrackerClient: mockClients.MockArtifact,
			}

			resp, err := vzClusterInfoServer.UpdateOrInstallCluster(ctx, &cloudpb.UpdateOrInstallClusterRequest{
				ClusterID: clusterID,
				Version:   "0.1.30",
			})

			require.NoError(t, err)
			assert.NotNil(t, resp)
		})
	}
}
