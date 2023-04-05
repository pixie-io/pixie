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
	"strings"
	"testing"
	"time"

	"cloud.google.com/go/storage"
	"github.com/gogo/protobuf/types"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/oauth2/jwt"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	apb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/artifact_tracker/controllers"
	"px.dev/pixie/src/shared/artifacts/manifest"
	vpb "px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/utils/testingutils"
)

func mustSetupFakeBucket(t *testing.T) stiface.Client {
	return testingutils.NewMockGCSClient(map[string]*testingutils.MockGCSBucket{
		"test-bucket": testingutils.NewMockGCSBucket(
			map[string]*testingutils.MockGCSObject{
				"cli/1.2.1-pre.3/cli_linux_amd64.sha256": testingutils.NewMockGCSObject([]byte("the-sha256"), nil),
				"cli/1.2.1-pre.3/cli_linux_amd64": testingutils.NewMockGCSObject([]byte("mybin"), &storage.ObjectAttrs{
					MediaLink: "the-url",
				}),
			},
			nil,
		),
	})
}

func loadTestManifest(s *controllers.Server) error {
	manifestStr := `
  [
    {
      "name": "cli",
      "artifact": [
        {
          "timestamp": "2019-06-22T19:10:25Z",
          "commit_hash": "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
          "version_str": "1.2.3",
          "available_artifacts": [
            "AT_LINUX_AMD64",
            "AT_DARWIN_AMD64"
          ],
          "changelog": "cl 0"
        },
        {
          "timestamp": "2019-06-22T19:10:25Z",
          "commit_hash": "ada4ac2f4c979e81f5d95a2b550a08fb041e985c",
          "version_str": "1.2.1-pre.3",
          "available_artifacts": [
            "AT_LINUX_AMD64"
          ],
          "changelog": "cl 1"
        },
        {
          "timestamp": "2019-06-21T19:10:25Z",
          "commit_hash": "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
          "version_str": "1.1.5",
          "available_artifacts": [
            "AT_LINUX_AMD64",
            "AT_DARWIN_AMD64"
          ],
          "changelog": "cl 2"
        }
      ]
    },
    {
      "name": "vizier",
      "artifact": [
        {
          "timestamp": "2019-06-21T19:10:25Z",
          "commit_hash": "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
          "version_str": "1.2.0",
          "available_artifacts": [
            "AT_CONTAINER_SET_LINUX_AMD64"
          ],
          "changelog": "cl2 0"
        },
        {
          "timestamp": "2019-06-21T17:10:25Z",
          "commit_hash": "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
          "version_str": "1.1.5",
          "available_artifacts": [
            "AT_CONTAINER_SET_LINUX_AMD64"
          ],
          "changelog": "cl2 1"
        }
      ]
    }
  ]
  `
	m, err := manifest.ReadArtifactManifest(strings.NewReader(manifestStr))
	if err != nil {
		return err
	}
	if err := s.UpdateManifest(m); err != nil {
		return err
	}
	return nil
}

func TestServer_GetArtifactList(t *testing.T) {
	server := controllers.NewServer(nil, "bucket", nil)

	err := loadTestManifest(server)
	require.NoError(t, err)

	testCases := []struct {
		name         string
		req          apb.GetArtifactListRequest
		expectedResp *vpb.ArtifactSet
		err          error
	}{
		{
			name: "cli linux limit 1 should return 1 linux artifact",
			req: apb.GetArtifactListRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        1,
			},
			expectedResp: &vpb.ArtifactSet{
				Name: "cli",
				Artifact: []*vpb.Artifact{
					{
						Timestamp:          &types.Timestamp{Seconds: 1561230625},
						CommitHash:         "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.2.3",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 0",
					},
				},
			},
			err: nil,
		},
		{
			name: "cli linux limit 0 should return 2 linux artifacts",
			req: apb.GetArtifactListRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        0,
			},
			expectedResp: &vpb.ArtifactSet{
				Name: "cli",
				Artifact: []*vpb.Artifact{
					{
						Timestamp:          &types.Timestamp{Seconds: 1561230625},
						CommitHash:         "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.2.3",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 0",
					},
					{
						Timestamp:          &types.Timestamp{Seconds: 1561144225},
						CommitHash:         "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.1.5",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 2",
					},
				},
			},
			err: nil,
		},
		{
			name: "cli linux limit 2 should return 2 linux artifacts",
			req: apb.GetArtifactListRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        2,
			},
			expectedResp: &vpb.ArtifactSet{
				Name: "cli",
				Artifact: []*vpb.Artifact{
					{
						Timestamp:          &types.Timestamp{Seconds: 1561230625},
						CommitHash:         "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.2.3",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 0",
					},
					{
						Timestamp:          &types.Timestamp{Seconds: 1561144225},
						CommitHash:         "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.1.5",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 2",
					},
				},
			},
			err: nil,
		},
		{
			name: "vizier limit 1 should return empty set",
			req: apb.GetArtifactListRequest{
				ArtifactName: "vizier",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        1,
			},
			expectedResp: &vpb.ArtifactSet{
				Name:     "vizier",
				Artifact: []*vpb.Artifact{},
			},
			err: nil,
		},
		{
			name: "missing artifact type is an error",
			req: apb.GetArtifactListRequest{
				ArtifactName: "vizier",
				Limit:        1,
			},
			expectedResp: nil,
			err:          status.Error(codes.InvalidArgument, "missing artifact type"),
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			resp, err := server.GetArtifactList(context.Background(), &tc.req)
			if tc.err != nil {
				assert.Equal(t, status.Code(err), status.Code(tc.err))
			} else {
				require.NoError(t, err)
				assert.Equal(t, resp, tc.expectedResp)
			}
		})
	}
}

func TestServer_GetDownloadLink(t *testing.T) {
	storageClient := mustSetupFakeBucket(t)

	server := controllers.NewServer(storageClient, "test-bucket", &jwt.Config{
		Email:      "test@test.com",
		PrivateKey: []byte("the-key"),
	})

	err := loadTestManifest(server)
	require.NoError(t, err)

	testCases := []struct {
		name         string
		req          apb.GetDownloadLinkRequest
		expectedResp *apb.GetDownloadLinkResponse
		errCode      codes.Code
	}{
		{
			name: "missing artifact name should give an error",
			req: apb.GetDownloadLinkRequest{
				VersionStr:   "2019.21.1",
				ArtifactType: vpb.AT_LINUX_AMD64,
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "missing version string should give an error",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "missing artifact type should give an error",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				VersionStr:   "2019.21.1",
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "not downloadable artifact should give an error",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				VersionStr:   "2019.21.1",
				ArtifactType: vpb.AT_CONTAINER_SET_LINUX_AMD64,
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "Linux CLI fetch",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				VersionStr:   "1.2.1-pre.3",
				ArtifactType: vpb.AT_LINUX_AMD64,
			},
			expectedResp: &apb.GetDownloadLinkResponse{
				Url:    "the-url",
				SHA256: "the-sha256",
			},
			errCode: codes.OK,
		},
	}
	// Only testing error cases for now because the storage API is hard to mock.
	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			resp, err := server.GetDownloadLink(context.Background(), &tc.req)
			if tc.errCode != codes.OK {
				assert.Equal(t, status.Code(err), tc.errCode)
				assert.Nil(t, resp)
			} else {
				require.NoError(t, err)
				ts, err := types.TimestampFromProto(resp.ValidUntil)
				require.NoError(t, err)
				assert.True(t, time.Until(ts) > 0)
				assert.Equal(t, resp.Url, tc.expectedResp.Url)
				assert.Equal(t, resp.SHA256, tc.expectedResp.SHA256)
			}
		})
	}
}
