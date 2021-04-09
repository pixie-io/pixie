package controller_test

import (
	"context"
	"reflect"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	public_cloudapipb "pixielabs.ai/pixielabs/src/api/public/cloudapipb"
	"pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/auth/authpb"
	"pixielabs.ai/pixielabs/src/cloud/autocomplete"
	mock_autocomplete "pixielabs.ai/pixielabs/src/cloud/autocomplete/mock"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb"
	mock_scriptmgr "pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb/mock"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	pl_vispb "pixielabs.ai/pixielabs/src/shared/vispb"
	pbutils "pixielabs.ai/pixielabs/src/utils"
)

func TestArtifactTracker_GetArtifactList(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetArtifactList(gomock.Any(),
		&artifacttrackerpb.GetArtifactListRequest{
			ArtifactName: "cli",
			Limit:        1,
			ArtifactType: versionspb.AT_LINUX_AMD64,
		}).
		Return(&versionspb.ArtifactSet{
			Name: "cli",
			Artifact: []*versionspb.Artifact{{
				VersionStr: "test",
			}},
		}, nil)

	artifactTrackerServer := &controller.ArtifactTrackerServer{
		ArtifactTrackerClient: mockClients.MockArtifact,
	}

	resp, err := artifactTrackerServer.GetArtifactList(ctx, &cloudapipb.GetArtifactListRequest{
		ArtifactName: "cli",
		Limit:        1,
		ArtifactType: cloudapipb.AT_LINUX_AMD64,
	})

	require.NoError(t, err)
	assert.Equal(t, "cli", resp.Name)
	assert.Equal(t, 1, len(resp.Artifact))
}

func TestArtifactTracker_GetDownloadLink(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetDownloadLink(gomock.Any(),
		&artifacttrackerpb.GetDownloadLinkRequest{
			ArtifactName: "cli",
			VersionStr:   "version",
			ArtifactType: versionspb.AT_LINUX_AMD64,
		}).
		Return(&artifacttrackerpb.GetDownloadLinkResponse{
			Url:    "http://localhost",
			SHA256: "sha",
		}, nil)

	artifactTrackerServer := &controller.ArtifactTrackerServer{
		ArtifactTrackerClient: mockClients.MockArtifact,
	}

	resp, err := artifactTrackerServer.GetDownloadLink(ctx, &cloudapipb.GetDownloadLinkRequest{
		ArtifactName: "cli",
		VersionStr:   "version",
		ArtifactType: cloudapipb.AT_LINUX_AMD64,
	})

	require.NoError(t, err)
	assert.Equal(t, "http://localhost", resp.Url)
	assert.Equal(t, "sha", resp.SHA256)
}

func TestVizierClusterInfo_GetClusterConnection(t *testing.T) {
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockVzMgr.EXPECT().GetVizierConnectionInfo(gomock.Any(), clusterID).Return(&cvmsgspb.VizierConnectionInfo{
		IPAddress: "127.0.0.1",
		Token:     "hello",
	}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockClients.MockVzMgr,
	}

	resp, err := vzClusterInfoServer.GetClusterConnection(ctx, &public_cloudapipb.GetClusterConnectionRequest{ID: clusterID})
	require.NoError(t, err)
	assert.Equal(t, "127.0.0.1", resp.IPAddress)
	assert.Equal(t, "hello", resp.Token)
}

func TestVizierClusterInfo_GetClusterInfo(t *testing.T) {
	orgID := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), orgID).Return(&vzmgrpb.GetViziersByOrgResponse{
		VizierIDs: []*uuidpb.UUID{clusterID},
	}, nil)

	mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: []*uuidpb.UUID{clusterID},
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{{
			VizierID:        clusterID,
			Status:          cvmsgspb.VZ_ST_HEALTHY,
			LastHeartbeatNs: int64(1305646598000000000),
			Config: &cvmsgspb.VizierConfig{
				PassthroughEnabled: false,
				AutoUpdateEnabled:  true,
			},
			VizierVersion:  "1.2.3",
			ClusterUID:     "a UID",
			ClusterName:    "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar-3",
			ClusterVersion: "5.6.7",
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

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockClients.MockVzMgr,
	}

	resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})

	expectedPodStatuses := map[string]*cloudapipb.PodStatus{
		"vizier-proxy": {
			Name:   "vizier-proxy",
			Status: metadatapb.RUNNING,
			Containers: []*cloudapipb.ContainerStatus{
				{
					Name:      "my-proxy-container",
					State:     metadatapb.CONTAINER_STATE_RUNNING,
					Message:   "container message",
					Reason:    "container reason",
					CreatedAt: &types.Timestamp{Seconds: 1561230620},
				},
			},
			Events: []*cloudapipb.K8SEvent{
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
			Status:    metadatapb.RUNNING,
			CreatedAt: nil,
		},
	}

	require.NoError(t, err)
	assert.Equal(t, 1, len(resp.Clusters))
	cluster := resp.Clusters[0]
	assert.Equal(t, cluster.ID, clusterID)
	assert.Equal(t, cluster.Status, cloudapipb.CS_HEALTHY)
	assert.Equal(t, cluster.LastHeartbeatNs, int64(1305646598000000000))
	assert.Equal(t, cluster.Config.PassthroughEnabled, false)
	assert.Equal(t, cluster.Config.AutoUpdateEnabled, true)
	assert.Equal(t, "1.2.3", cluster.VizierVersion)
	assert.Equal(t, "a UID", cluster.ClusterUID)
	assert.Equal(t, "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar-3", cluster.ClusterName)
	assert.Equal(t, "gke:dev-cluster-zasgar-3", cluster.PrettyClusterName)
	assert.Equal(t, "5.6.7", cluster.ClusterVersion)
	assert.Equal(t, expectedPodStatuses, cluster.ControlPlanePodStatuses)
	assert.Equal(t, int32(5), cluster.NumNodes)
	assert.Equal(t, int32(3), cluster.NumInstrumentedNodes)
}

func TestVizierClusterInfo_GetClusterInfoDuplicates(t *testing.T) {
	orgID := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)
	clusterID2 := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9")

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), orgID).Return(&vzmgrpb.GetViziersByOrgResponse{
		VizierIDs: []*uuidpb.UUID{clusterID, clusterID2},
	}, nil)

	mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: []*uuidpb.UUID{clusterID, clusterID2},
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{{
			VizierID:        clusterID,
			Status:          cvmsgspb.VZ_ST_HEALTHY,
			LastHeartbeatNs: int64(1305646598000000000),
			Config: &cvmsgspb.VizierConfig{
				PassthroughEnabled: false,
				AutoUpdateEnabled:  true,
			},
			VizierVersion:        "1.2.3",
			ClusterUID:           "a UID",
			ClusterName:          "gke_pl-dev-infra_us-west1-a_dev-cluster-zasgar",
			ClusterVersion:       "5.6.7",
			NumNodes:             5,
			NumInstrumentedNodes: 3,
		},
			{
				VizierID:        clusterID,
				Status:          cvmsgspb.VZ_ST_HEALTHY,
				LastHeartbeatNs: int64(1305646598000000000),
				Config: &cvmsgspb.VizierConfig{
					PassthroughEnabled: false,
					AutoUpdateEnabled:  true,
				},
				VizierVersion:        "1.2.3",
				ClusterUID:           "a UID2",
				ClusterName:          "gke_pl-pixies_us-west1-a_dev-cluster-zasgar",
				ClusterVersion:       "5.6.7",
				NumNodes:             5,
				NumInstrumentedNodes: 3,
			},
		},
	}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockClients.MockVzMgr,
	}

	resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})

	require.NoError(t, err)
	assert.Equal(t, 2, len(resp.Clusters))
	assert.Equal(t, "gke:dev-cluster-zasgar (pl-dev-infra)", resp.Clusters[0].PrettyClusterName)
	assert.Equal(t, "gke:dev-cluster-zasgar (pl-pixies)", resp.Clusters[1].PrettyClusterName)
}

func TestVizierClusterInfo_GetClusterInfoWithID(t *testing.T) {
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockVzMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: []*uuidpb.UUID{clusterID},
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{{
			VizierID:        clusterID,
			Status:          cvmsgspb.VZ_ST_HEALTHY,
			LastHeartbeatNs: int64(1305646598000000000),
			Config: &cvmsgspb.VizierConfig{
				PassthroughEnabled: false,
				AutoUpdateEnabled:  true,
			},
			VizierVersion:  "1.2.3",
			ClusterUID:     "a UID",
			ClusterName:    "some cluster",
			ClusterVersion: "5.6.7",
		},
		},
	}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockClients.MockVzMgr,
	}

	resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{
		ID: clusterID,
	})

	require.NoError(t, err)
	assert.Equal(t, 1, len(resp.Clusters))
	cluster := resp.Clusters[0]
	assert.Equal(t, cluster.ID, clusterID)
	assert.Equal(t, cluster.Status, cloudapipb.CS_HEALTHY)
	assert.Equal(t, cluster.LastHeartbeatNs, int64(1305646598000000000))
	assert.Equal(t, cluster.Config.PassthroughEnabled, false)
	assert.Equal(t, cluster.Config.AutoUpdateEnabled, true)
}

func TestVizierClusterInfo_UpdateClusterVizierConfig(t *testing.T) {
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	updateReq := &cvmsgspb.UpdateVizierConfigRequest{
		VizierID: clusterID,
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{
			PassthroughEnabled: &types.BoolValue{Value: true},
			AutoUpdateEnabled:  &types.BoolValue{Value: false},
		},
	}

	mockClients.MockVzMgr.EXPECT().UpdateVizierConfig(gomock.Any(), updateReq).Return(&cvmsgspb.UpdateVizierConfigResponse{}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockClients.MockVzMgr,
	}

	resp, err := vzClusterInfoServer.UpdateClusterVizierConfig(ctx, &cloudapipb.UpdateClusterVizierConfigRequest{
		ID: clusterID,
		ConfigUpdate: &cloudapipb.VizierConfigUpdate{
			PassthroughEnabled: &types.BoolValue{Value: true},
			AutoUpdateEnabled:  &types.BoolValue{Value: false},
		},
	})

	require.NoError(t, err)
	assert.NotNil(t, resp)
}

func TestVizierClusterInfo_UpdateOrInstallCluster(t *testing.T) {
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

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

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr:                 mockClients.MockVzMgr,
		ArtifactTrackerClient: mockClients.MockArtifact,
	}

	resp, err := vzClusterInfoServer.UpdateOrInstallCluster(ctx, &cloudapipb.UpdateOrInstallClusterRequest{
		ClusterID: clusterID,
		Version:   "0.1.30",
	})

	require.NoError(t, err)
	assert.NotNil(t, resp)
}

func TestVizierDeploymentKeyServer_Create(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzreq := &vzmgrpb.CreateDeploymentKeyRequest{Desc: "test key"}
	vzresp := &vzmgrpb.DeploymentKey{
		ID:        pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Key:       "foobar",
		CreatedAt: types.TimestampNow(),
	}
	mockClients.MockVzDeployKey.EXPECT().
		Create(gomock.Any(), vzreq).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}

	resp, err := vzDeployKeyServer.Create(ctx, &cloudapipb.CreateDeploymentKeyRequest{Desc: "test key"})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.ID, vzresp.ID)
	assert.Equal(t, resp.Key, vzresp.Key)
	assert.Equal(t, resp.CreatedAt, vzresp.CreatedAt)
}

func TestVizierDeploymentKeyServer_List(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzreq := &vzmgrpb.ListDeploymentKeyRequest{}
	vzresp := &vzmgrpb.ListDeploymentKeyResponse{
		Keys: []*vzmgrpb.DeploymentKey{
			{
				ID:        pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Key:       "foobar",
				CreatedAt: types.TimestampNow(),
				Desc:      "this is a key",
			},
		},
	}
	mockClients.MockVzDeployKey.EXPECT().
		List(gomock.Any(), vzreq).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}

	resp, err := vzDeployKeyServer.List(ctx, &cloudapipb.ListDeploymentKeyRequest{})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	for i, key := range resp.Keys {
		assert.Equal(t, key.ID, vzresp.Keys[i].ID)
		assert.Equal(t, key.Key, vzresp.Keys[i].Key)
		assert.Equal(t, key.CreatedAt, vzresp.Keys[i].CreatedAt)
		assert.Equal(t, key.Desc, vzresp.Keys[i].Desc)
	}
}

func TestVizierDeploymentKeyServer_Get(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	vzreq := &vzmgrpb.GetDeploymentKeyRequest{
		ID: id,
	}
	vzresp := &vzmgrpb.GetDeploymentKeyResponse{
		Key: &vzmgrpb.DeploymentKey{
			ID:        id,
			Key:       "foobar",
			CreatedAt: types.TimestampNow(),
			Desc:      "this is a key",
		},
	}
	mockClients.MockVzDeployKey.EXPECT().
		Get(gomock.Any(), vzreq).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}
	resp, err := vzDeployKeyServer.Get(ctx, &cloudapipb.GetDeploymentKeyRequest{
		ID: id,
	})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.Key.ID, vzresp.Key.ID)
	assert.Equal(t, resp.Key.Key, vzresp.Key.Key)
	assert.Equal(t, resp.Key.CreatedAt, vzresp.Key.CreatedAt)
	assert.Equal(t, resp.Key.Desc, vzresp.Key.Desc)
}

func TestVizierDeploymentKeyServer_Delete(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	vzresp := &types.Empty{}
	mockClients.MockVzDeployKey.EXPECT().
		Delete(gomock.Any(), id).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}
	resp, err := vzDeployKeyServer.Delete(ctx, id)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp, vzresp)
}

func TestAPIKeyServer_Create(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzreq := &authpb.CreateAPIKeyRequest{Desc: "test key"}
	vzresp := &authpb.APIKey{
		ID:        pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Key:       "foobar",
		CreatedAt: types.TimestampNow(),
	}
	mockClients.MockAPIKey.EXPECT().
		Create(gomock.Any(), vzreq).Return(vzresp, nil)

	vzAPIKeyServer := &controller.APIKeyServer{
		APIKeyClient: mockClients.MockAPIKey,
	}

	resp, err := vzAPIKeyServer.Create(ctx, &cloudapipb.CreateAPIKeyRequest{Desc: "test key"})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.ID, vzresp.ID)
	assert.Equal(t, resp.Key, vzresp.Key)
	assert.Equal(t, resp.CreatedAt, vzresp.CreatedAt)
}

func TestAPIKeyServer_List(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzreq := &authpb.ListAPIKeyRequest{}
	vzresp := &authpb.ListAPIKeyResponse{
		Keys: []*authpb.APIKey{
			{
				ID:        pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Key:       "foobar",
				CreatedAt: types.TimestampNow(),
				Desc:      "this is a key",
			},
		},
	}
	mockClients.MockAPIKey.EXPECT().
		List(gomock.Any(), vzreq).Return(vzresp, nil)

	vzAPIKeyServer := &controller.APIKeyServer{
		APIKeyClient: mockClients.MockAPIKey,
	}

	resp, err := vzAPIKeyServer.List(ctx, &cloudapipb.ListAPIKeyRequest{})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	for i, key := range resp.Keys {
		assert.Equal(t, key.ID, vzresp.Keys[i].ID)
		assert.Equal(t, key.Key, vzresp.Keys[i].Key)
		assert.Equal(t, key.CreatedAt, vzresp.Keys[i].CreatedAt)
		assert.Equal(t, key.Desc, vzresp.Keys[i].Desc)
	}
}

func TestAPIKeyServer_Get(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	vzreq := &authpb.GetAPIKeyRequest{
		ID: id,
	}
	vzresp := &authpb.GetAPIKeyResponse{
		Key: &authpb.APIKey{
			ID:        id,
			Key:       "foobar",
			CreatedAt: types.TimestampNow(),
			Desc:      "this is a key",
		},
	}
	mockClients.MockAPIKey.EXPECT().
		Get(gomock.Any(), vzreq).Return(vzresp, nil)

	vzAPIKeyServer := &controller.APIKeyServer{
		APIKeyClient: mockClients.MockAPIKey,
	}
	resp, err := vzAPIKeyServer.Get(ctx, &cloudapipb.GetAPIKeyRequest{
		ID: id,
	})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.Key.ID, vzresp.Key.ID)
	assert.Equal(t, resp.Key.Key, vzresp.Key.Key)
	assert.Equal(t, resp.Key.CreatedAt, vzresp.Key.CreatedAt)
	assert.Equal(t, resp.Key.Desc, vzresp.Key.Desc)
}

func TestAPIKeyServer_Delete(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	vzresp := &types.Empty{}
	mockClients.MockAPIKey.EXPECT().
		Delete(gomock.Any(), id).Return(vzresp, nil)

	vzAPIKeyServer := &controller.APIKeyServer{
		APIKeyClient: mockClients.MockAPIKey,
	}
	resp, err := vzAPIKeyServer.Delete(ctx, id)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp, vzresp)
}

func TestAutocompleteService_Autocomplete(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID, err := uuid.FromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	require.NoError(t, err)
	ctx := CreateTestContext()

	s := mock_autocomplete.NewMockSuggester(ctrl)

	requests := [][]*autocomplete.SuggestionRequest{
		{
			{
				OrgID:        orgID,
				ClusterUID:   "test",
				Input:        "px/svc_info",
				AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE, cloudapipb.AEK_SCRIPT},
				AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
			},
			{
				OrgID:        orgID,
				ClusterUID:   "test",
				Input:        "pl/test",
				AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE, cloudapipb.AEK_SCRIPT},
				AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
			},
		},
	}

	responses := [][]*autocomplete.SuggestionResult{
		{
			{
				Suggestions: []*autocomplete.Suggestion{
					{
						Name:     "px/svc_info",
						Score:    1,
						ArgNames: []string{"svc_name"},
						ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					},
				},
				ExactMatch: true,
			},
			{
				Suggestions: []*autocomplete.Suggestion{
					{
						Name:  "px/test",
						Score: 1,
					},
				},
				ExactMatch: true,
			},
		},
	}

	suggestionCalls := 0
	s.EXPECT().
		GetSuggestions(gomock.Any()).
		DoAndReturn(func(req []*autocomplete.SuggestionRequest) ([]*autocomplete.SuggestionResult, error) {
			assert.ElementsMatch(t, requests[suggestionCalls], req)
			resp := responses[suggestionCalls]
			suggestionCalls++
			return resp, nil
		}).
		Times(len(requests))

	autocompleteServer := &controller.AutocompleteServer{
		Suggester: s,
	}

	resp, err := autocompleteServer.Autocomplete(ctx, &cloudapipb.AutocompleteRequest{
		Input:      "px/svc_info pl/test",
		CursorPos:  0,
		Action:     cloudapipb.AAT_EDIT,
		ClusterUID: "test",
	})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "${2:$0px/svc_info} ${1:pl/test}", resp.FormattedInput)
	assert.False(t, resp.IsExecutable)
	assert.Equal(t, 2, len(resp.TabSuggestions))
}

func TestAutocompleteService_AutocompleteField(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID, err := uuid.FromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	require.NoError(t, err)
	ctx := CreateTestContext()

	s := mock_autocomplete.NewMockSuggester(ctrl)

	requests := [][]*autocomplete.SuggestionRequest{
		{
			{
				OrgID:        orgID,
				ClusterUID:   "test",
				Input:        "px/svc_info",
				AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
				AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
			},
		},
	}

	responses := []*autocomplete.SuggestionResult{
		{
			Suggestions: []*autocomplete.Suggestion{
				{
					Name:  "px/svc_info",
					Score: 1,
					State: cloudapipb.AES_RUNNING,
				},
				{
					Name:  "px/svc_info2",
					Score: 1,
					State: cloudapipb.AES_TERMINATED,
				},
			},
			ExactMatch: true,
		},
	}

	s.EXPECT().
		GetSuggestions(gomock.Any()).
		DoAndReturn(func(req []*autocomplete.SuggestionRequest) ([]*autocomplete.SuggestionResult, error) {
			assert.ElementsMatch(t, requests[0], req)
			return responses, nil
		})

	autocompleteServer := &controller.AutocompleteServer{
		Suggester: s,
	}

	resp, err := autocompleteServer.AutocompleteField(ctx, &cloudapipb.AutocompleteFieldRequest{
		Input:      "px/svc_info",
		FieldType:  cloudapipb.AEK_SVC,
		ClusterUID: "test",
	})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, 2, len(resp.Suggestions))
}

func toBytes(t *testing.T, msg proto.Message) []byte {
	bytes, err := proto.Marshal(msg)
	require.NoError(t, err)
	return bytes
}

func TestScriptMgr(t *testing.T) {
	var testVis = &pl_vispb.Vis{
		Widgets: []*pl_vispb.Widget{
			{
				FuncOrRef: &pl_vispb.Widget_Func_{
					Func: &pl_vispb.Widget_Func{
						Name: "my_func",
					},
				},
				DisplaySpec: &types.Any{
					TypeUrl: "pixielabs.ai/pl.vispb.VegaChart",
					Value: toBytes(t, &pl_vispb.VegaChart{
						Spec: "{}",
					}),
				},
			},
		},
	}

	ID1 := uuid.Must(uuid.NewV4())
	ID2 := uuid.Must(uuid.NewV4())
	testCases := []struct {
		name         string
		endpoint     string
		smReq        proto.Message
		smResp       proto.Message
		req          proto.Message
		expectedResp proto.Message
	}{
		{
			name:     "GetLiveViews correctly translates from scriptmgrpb to cloudapipb.",
			endpoint: "GetLiveViews",
			smReq:    &scriptmgrpb.GetLiveViewsReq{},
			smResp: &scriptmgrpb.GetLiveViewsResp{
				LiveViews: []*scriptmgrpb.LiveViewMetadata{
					{
						ID:   pbutils.ProtoFromUUID(ID1),
						Name: "liveview1",
						Desc: "liveview1 desc",
					},
					{
						ID:   pbutils.ProtoFromUUID(ID2),
						Name: "liveview2",
						Desc: "liveview2 desc",
					},
				},
			},
			req: &cloudapipb.GetLiveViewsReq{},
			expectedResp: &cloudapipb.GetLiveViewsResp{
				LiveViews: []*cloudapipb.LiveViewMetadata{
					{
						ID:   ID1.String(),
						Name: "liveview1",
						Desc: "liveview1 desc",
					},
					{
						ID:   ID2.String(),
						Name: "liveview2",
						Desc: "liveview2 desc",
					},
				},
			},
		},
		{
			name:     "GetLiveViewContents correctly translates between scriptmgr and cloudapipb.",
			endpoint: "GetLiveViewContents",
			smReq: &scriptmgrpb.GetLiveViewContentsReq{
				LiveViewID: pbutils.ProtoFromUUID(ID1),
			},
			smResp: &scriptmgrpb.GetLiveViewContentsResp{
				Metadata: &scriptmgrpb.LiveViewMetadata{
					ID:   pbutils.ProtoFromUUID(ID1),
					Name: "liveview1",
					Desc: "liveview1 desc",
				},
				PxlContents: "liveview1 pxl",
				Vis:         testVis,
			},
			req: &cloudapipb.GetLiveViewContentsReq{
				LiveViewID: ID1.String(),
			},
			expectedResp: &cloudapipb.GetLiveViewContentsResp{
				Metadata: &cloudapipb.LiveViewMetadata{
					ID:   ID1.String(),
					Name: "liveview1",
					Desc: "liveview1 desc",
				},
				PxlContents: "liveview1 pxl",
				Vis:         testVis,
			},
		},
		{
			name:     "GetScripts correctly translates between scriptmgr and cloudapipb.",
			endpoint: "GetScripts",
			smReq:    &scriptmgrpb.GetScriptsReq{},
			smResp: &scriptmgrpb.GetScriptsResp{
				Scripts: []*scriptmgrpb.ScriptMetadata{
					{
						ID:          pbutils.ProtoFromUUID(ID1),
						Name:        "script1",
						Desc:        "script1 desc",
						HasLiveView: false,
					},
					{
						ID:          pbutils.ProtoFromUUID(ID2),
						Name:        "liveview1",
						Desc:        "liveview1 desc",
						HasLiveView: true,
					},
				},
			},
			req: &cloudapipb.GetScriptsReq{},
			expectedResp: &cloudapipb.GetScriptsResp{
				Scripts: []*cloudapipb.ScriptMetadata{
					{
						ID:          ID1.String(),
						Name:        "script1",
						Desc:        "script1 desc",
						HasLiveView: false,
					},
					{
						ID:          ID2.String(),
						Name:        "liveview1",
						Desc:        "liveview1 desc",
						HasLiveView: true,
					},
				},
			},
		},
		{
			name:     "GetScriptContents correctly translates between scriptmgr and cloudapipb.",
			endpoint: "GetScriptContents",
			smReq: &scriptmgrpb.GetScriptContentsReq{
				ScriptID: pbutils.ProtoFromUUID(ID1),
			},
			smResp: &scriptmgrpb.GetScriptContentsResp{
				Metadata: &scriptmgrpb.ScriptMetadata{
					ID:          pbutils.ProtoFromUUID(ID1),
					Name:        "Script1",
					Desc:        "Script1 desc",
					HasLiveView: false,
				},
				Contents: "Script1 pxl",
			},
			req: &cloudapipb.GetScriptContentsReq{
				ScriptID: ID1.String(),
			},
			expectedResp: &cloudapipb.GetScriptContentsResp{
				Metadata: &cloudapipb.ScriptMetadata{
					ID:          ID1.String(),
					Name:        "Script1",
					Desc:        "Script1 desc",
					HasLiveView: false,
				},
				Contents: "Script1 pxl",
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			mockScriptMgr := mock_scriptmgr.NewMockScriptMgrServiceClient(ctrl)
			ctx := CreateTestContext()

			reflect.ValueOf(mockScriptMgr.EXPECT()).
				MethodByName(tc.endpoint).
				Call([]reflect.Value{
					reflect.ValueOf(gomock.Any()),
					reflect.ValueOf(tc.smReq),
				})[0].Interface().(*gomock.Call).
				Return(tc.smResp, nil)

			scriptMgrServer := &controller.ScriptMgrServer{
				ScriptMgr: mockScriptMgr,
			}

			returnVals := reflect.ValueOf(scriptMgrServer).
				MethodByName(tc.endpoint).
				Call([]reflect.Value{
					reflect.ValueOf(ctx),
					reflect.ValueOf(tc.req),
				})
			assert.Nil(t, returnVals[1].Interface())
			resp := returnVals[0].Interface().(proto.Message)

			assert.Equal(t, tc.expectedResp, resp)
		})
	}
}

func TestProfileServer_GetOrgInfo(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	orgID := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockProfile.EXPECT().GetOrg(gomock.Any(), orgID).
		Return(&profilepb.OrgInfo{
			OrgName: "someOrg",
			ID:      orgID,
		}, nil)

	profileServer := &controller.ProfileServer{mockClients.MockProfile}

	resp, err := profileServer.GetOrgInfo(ctx, orgID)

	require.NoError(t, err)
	assert.Equal(t, "someOrg", resp.OrgName)
	assert.Equal(t, orgID, resp.ID)
}
