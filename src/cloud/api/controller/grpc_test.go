package controller_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	versionspb "pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
)

func TestArtifactTracker_GetArtifactList(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, _, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
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

	_, _, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
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
