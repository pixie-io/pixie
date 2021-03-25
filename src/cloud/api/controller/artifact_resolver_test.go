package controller_test

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

func TestCLIArtifact(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetArtifactList(gomock.Any(),
		&cloudapipb.GetArtifactListRequest{
			ArtifactName: "cli",
			Limit:        1,
			ArtifactType: cloudapipb.AT_LINUX_AMD64,
		}).
		Return(&cloudapipb.ArtifactSet{
			Name: "cli",
			Artifact: []*cloudapipb.Artifact{{
				VersionStr: "test",
			}},
		}, nil)

	mockClients.MockArtifact.EXPECT().GetDownloadLink(gomock.Any(), &cloudapipb.GetDownloadLinkRequest{
		ArtifactType: cloudapipb.AT_LINUX_AMD64,
		VersionStr:   "test",
		ArtifactName: "cli",
	}).Return(&cloudapipb.GetDownloadLinkResponse{
		Url:    "http://pixie.com/cli_url",
		SHA256: "http://pixie.com/sha256",
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
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
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetArtifactList(gomock.Any(),
		&cloudapipb.GetArtifactListRequest{
			ArtifactName: "cli",
			ArtifactType: cloudapipb.AT_LINUX_AMD64,
		}).
		Return(&cloudapipb.ArtifactSet{
			Name: "cli",
			Artifact: []*cloudapipb.Artifact{{
				VersionStr: "1.2.3",
				Changelog:  "a changelog",
				Timestamp:  &types.Timestamp{Seconds: 10},
			}, {
				VersionStr: "1.2.2",
				Changelog:  "some changes go here",
				Timestamp:  &types.Timestamp{Seconds: 5},
			}},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
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
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetArtifactList(gomock.Any(),
		&cloudapipb.GetArtifactListRequest{
			ArtifactName: "vizier",
			ArtifactType: cloudapipb.AT_CONTAINER_SET_LINUX_AMD64,
		}).
		Return(&cloudapipb.ArtifactSet{
			Name: "vizier",
			Artifact: []*cloudapipb.Artifact{{
				VersionStr: "1.2.3",
				Changelog:  "a changelog",
				Timestamp:  &types.Timestamp{Seconds: 10},
			}, {
				VersionStr: "1.2.2",
				Changelog:  "some changes go here",
				Timestamp:  &types.Timestamp{Seconds: 5},
			}},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
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
