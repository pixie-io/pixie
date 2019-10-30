package controller_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"github.com/gogo/protobuf/types"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	versionspb "pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
)

func TestCLIArtifact(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	apiEnv, _, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
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

	mockArtifactClient.EXPECT().GetDownloadLink(gomock.Any(), &artifacttrackerpb.GetDownloadLinkRequest{
		ArtifactType: versionspb.AT_LINUX_AMD64,
		VersionStr:   "test",
		ArtifactName: "cli",
	}).Return(&artifacttrackerpb.GetDownloadLinkResponse{
		Url:    "http://pixie.com/cli_url",
		SHA256: "http://pixie.com/sha256",
	}, nil)

	gqlSchema := LoadSchema(apiEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					cliArtifact(artifactType: AT_LINUX_AMD64) {
						url
						sha256
					}
				}
			`,
			ExpectedResult: `
				{
					"cliArtifact": {
						"url":"http://pixie.com/cli_url",
						"sha256": "http://pixie.com/sha256"
					}
				}
			`,
		},
	})
}

func TestArtifacts_CLI(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	apiEnv, _, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockArtifactClient.EXPECT().GetArtifactList(gomock.Any(),
		&artifacttrackerpb.GetArtifactListRequest{
			ArtifactName: "cli",
			ArtifactType: versionspb.AT_LINUX_AMD64,
		}).
		Return(&versionspb.ArtifactSet{
			Name: "cli",
			Artifact: []*versionspb.Artifact{&versionspb.Artifact{
				VersionStr: "1.2.3",
				Changelog:  "a changelog",
				Timestamp:  &types.Timestamp{Seconds: 10},
			}, &versionspb.Artifact{
				VersionStr: "1.2.2",
				Changelog:  "some changes go here",
				Timestamp:  &types.Timestamp{Seconds: 5},
			}},
		}, nil)

	gqlSchema := LoadSchema(apiEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					artifacts(artifactName: "cli") {
						items {
							version
							changelog
							timestampMs
						}
					}
				}
			`,
			ExpectedResult: `
				{
					"artifacts": {
						"items": [
							{
								"version": "1.2.3",
								"changelog": "a changelog",
								"timestampMs": 10000
							},
							{
								"version": "1.2.2",
								"changelog": "some changes go here",
								"timestampMs": 5000
							}
						]
					}
				}
			`,
		},
	})
}

func TestArtifacts_Vizier(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	apiEnv, _, _, _, _, mockArtifactClient, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockArtifactClient.EXPECT().GetArtifactList(gomock.Any(),
		&artifacttrackerpb.GetArtifactListRequest{
			ArtifactName: "vizier",
			ArtifactType: versionspb.AT_CONTAINER_SET_LINUX_AMD64,
		}).
		Return(&versionspb.ArtifactSet{
			Name: "vizier",
			Artifact: []*versionspb.Artifact{&versionspb.Artifact{
				VersionStr: "1.2.3",
				Changelog:  "a changelog",
				Timestamp:  &types.Timestamp{Seconds: 10},
			}, &versionspb.Artifact{
				VersionStr: "1.2.2",
				Changelog:  "some changes go here",
				Timestamp:  &types.Timestamp{Seconds: 5},
			}},
		}, nil)

	gqlSchema := LoadSchema(apiEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					artifacts(artifactName: "vizier") {
						items {
							version
							changelog
							timestampMs
						}
					}
				}
			`,
			ExpectedResult: `
				{
					"artifacts": {
						"items": [
							{
								"version": "1.2.3",
								"changelog": "a changelog",
								"timestampMs": 10000
							},
							{
								"version": "1.2.2",
								"changelog": "some changes go here",
								"timestampMs": 5000
							}
						]
					}
				}
			`,
		},
	})
}
