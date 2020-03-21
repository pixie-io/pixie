package controller_test

import (
	"context"
	"testing"

	types "github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	versionspb "pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	pbutils "pixielabs.ai/pixielabs/src/utils"
)

func TestArtifactTracker_GetArtifactList(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockArtifactClient.EXPECT().GetArtifactList(gomock.Any(),
		&artifacttrackerpb.GetArtifactListRequest{
			ArtifactName: "cli",
			Limit:        1,
			ArtifactType: versionspb.AT_LINUX_AMD64,
		}).
		Return(&versionspb.ArtifactSet{
			Name: "cli",
			Artifact: []*versionspb.Artifact{&versionspb.Artifact{
				VersionStr: "test",
			}},
		}, nil)

	artifactTrackerServer := &controller.ArtifactTrackerServer{
		ArtifactTrackerClient: mockArtifactClient,
	}

	resp, err := artifactTrackerServer.GetArtifactList(ctx, &cloudapipb.GetArtifactListRequest{
		ArtifactName: "cli",
		Limit:        1,
		ArtifactType: cloudapipb.AT_LINUX_AMD64,
	})

	assert.Nil(t, err)
	assert.Equal(t, "cli", resp.Name)
	assert.Equal(t, 1, len(resp.Artifact))
}

func TestArtifactTracker_GetDownloadLink(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockArtifactClient.EXPECT().GetDownloadLink(gomock.Any(),
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
		ArtifactTrackerClient: mockArtifactClient,
	}

	resp, err := artifactTrackerServer.GetDownloadLink(ctx, &cloudapipb.GetDownloadLinkRequest{
		ArtifactName: "cli",
		VersionStr:   "version",
		ArtifactType: cloudapipb.AT_LINUX_AMD64,
	})

	assert.Nil(t, err)
	assert.Equal(t, "http://localhost", resp.Url)
	assert.Equal(t, "sha", resp.SHA256)
}

func TestVizierClusterInfo_CreateCluster(t *testing.T) {
	orgID := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, mockVzMgr, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	ccReq := &vzmgrpb.CreateVizierClusterRequest{
		OrgID: orgID,
	}
	mockVzMgr.EXPECT().CreateVizierCluster(gomock.Any(), ccReq).Return(clusterID, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockVzMgr,
	}

	resp, err := vzClusterInfoServer.CreateCluster(ctx, &cloudapipb.CreateClusterRequest{})
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.ClusterID, clusterID)
}

func TestVizierClusterInfo_GetClusterInfo(t *testing.T) {
	orgID := pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, mockVzMgr, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), orgID).Return(&vzmgrpb.GetViziersByOrgResponse{
		VizierIDs: []*uuidpb.UUID{clusterID},
	}, nil)

	mockVzMgr.EXPECT().GetVizierInfo(gomock.Any(), clusterID).Return(&cvmsgspb.VizierInfo{
		VizierID:        clusterID,
		Status:          cvmsgspb.VZ_ST_HEALTHY,
		LastHeartbeatNs: int64(1305646598000000000),
		Config: &cvmsgspb.VizierConfig{
			PassthroughEnabled: false,
		},
	}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockVzMgr,
	}

	resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})

	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp.Clusters))
	cluster := resp.Clusters[0]
	assert.Equal(t, cluster.ID, clusterID)
	assert.Equal(t, cluster.Status, cloudapipb.CS_HEALTHY)
	assert.Equal(t, cluster.LastHeartbeatNs, int64(1305646598000000000))
	assert.Equal(t, cluster.Config.PassthroughEnabled, false)
}

func TestVizierClusterInfo_GetClusterInfoWithID(t *testing.T) {
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, mockVzMgr, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockVzMgr.EXPECT().GetVizierInfo(gomock.Any(), clusterID).Return(&cvmsgspb.VizierInfo{
		VizierID:        clusterID,
		Status:          cvmsgspb.VZ_ST_HEALTHY,
		LastHeartbeatNs: int64(1305646598000000000),
		Config: &cvmsgspb.VizierConfig{
			PassthroughEnabled: false,
		},
	}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockVzMgr,
	}

	resp, err := vzClusterInfoServer.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{
		ID: clusterID,
	})

	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp.Clusters))
	cluster := resp.Clusters[0]
	assert.Equal(t, cluster.ID, clusterID)
	assert.Equal(t, cluster.Status, cloudapipb.CS_HEALTHY)
	assert.Equal(t, cluster.LastHeartbeatNs, int64(1305646598000000000))
	assert.Equal(t, cluster.Config.PassthroughEnabled, false)
}

func TestVizierClusterInfo_UpdateClusterVizierConfig(t *testing.T) {
	clusterID := pbutils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	assert.NotNil(t, clusterID)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, mockVzMgr, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	updateReq := &cvmsgspb.UpdateVizierConfigRequest{
		VizierID: clusterID,
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{
			PassthroughEnabled: &types.BoolValue{Value: true},
		},
	}

	mockVzMgr.EXPECT().UpdateVizierConfig(gomock.Any(), updateReq).Return(&cvmsgspb.UpdateVizierConfigResponse{}, nil)

	vzClusterInfoServer := &controller.VizierClusterInfo{
		VzMgr: mockVzMgr,
	}

	resp, err := vzClusterInfoServer.UpdateClusterVizierConfig(ctx, &cloudapipb.UpdateClusterVizierConfigRequest{
		ID: clusterID,
		ConfigUpdate: &cloudapipb.VizierConfigUpdate{
			PassthroughEnabled: &types.BoolValue{Value: true},
		},
	})

	assert.Nil(t, err)
	assert.NotNil(t, resp)
}
