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

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/shared/artifacts/versionspb"
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
			Artifact: []*versionspb.Artifact{
				{
					VersionStr: "test",
				},
				{
					VersionStr: "test2",
					AvailableArtifactMirrors: []*versionspb.ArtifactMirrors{
						{
							ArtifactType: versionspb.AT_LINUX_AMD64,
							SHA256:       "abcd",
							URLs: []string{
								"url1",
								"url2",
							},
						},
					},
				},
			},
		}, nil)

	artifactTrackerServer := &controllers.ArtifactTrackerServer{
		ArtifactTrackerClient: mockClients.MockArtifact,
	}

	resp, err := artifactTrackerServer.GetArtifactList(ctx, &cloudpb.GetArtifactListRequest{
		ArtifactName: "cli",
		Limit:        1,
		ArtifactType: cloudpb.AT_LINUX_AMD64,
	})

	require.NoError(t, err)
	assert.Equal(t, "cli", resp.Name)
	assert.Equal(t, 2, len(resp.Artifact))
	expectedArtifacts := []*cloudpb.Artifact{
		{
			VersionStr:               "test",
			AvailableArtifacts:       []cloudpb.ArtifactType{},
			AvailableArtifactMirrors: []*cloudpb.ArtifactMirrors{},
		},
		{
			VersionStr:         "test2",
			AvailableArtifacts: []cloudpb.ArtifactType{},
			AvailableArtifactMirrors: []*cloudpb.ArtifactMirrors{
				{
					ArtifactType: cloudpb.AT_LINUX_AMD64,
					SHA256:       "abcd",
					URLs: []string{
						"url1",
						"url2",
					},
				},
			},
		},
	}
	assert.Equal(t, expectedArtifacts, resp.Artifact)
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

	artifactTrackerServer := &controllers.ArtifactTrackerServer{
		ArtifactTrackerClient: mockClients.MockArtifact,
	}

	resp, err := artifactTrackerServer.GetDownloadLink(ctx, &cloudpb.GetDownloadLinkRequest{
		ArtifactName: "cli",
		VersionStr:   "version",
		ArtifactType: cloudpb.AT_LINUX_AMD64,
	})

	require.NoError(t, err)
	assert.Equal(t, "http://localhost", resp.Url)
	assert.Equal(t, "sha", resp.SHA256)
}
