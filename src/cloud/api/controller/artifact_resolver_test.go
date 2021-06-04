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

package controller_test

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controller"
	"px.dev/pixie/src/cloud/api/controller/schema/noauth"
	"px.dev/pixie/src/cloud/api/controller/testutils"
)

func TestCLIArtifact(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetArtifactList(gomock.Any(),
		&cloudpb.GetArtifactListRequest{
			ArtifactName: "cli",
			Limit:        1,
			ArtifactType: cloudpb.AT_LINUX_AMD64,
		}).
		Return(&cloudpb.ArtifactSet{
			Name: "cli",
			Artifact: []*cloudpb.Artifact{{
				VersionStr: "test",
			}},
		}, nil)

	mockClients.MockArtifact.EXPECT().GetDownloadLink(gomock.Any(), &cloudpb.GetDownloadLinkRequest{
		ArtifactType: cloudpb.AT_LINUX_AMD64,
		VersionStr:   "test",
		ArtifactName: "cli",
	}).Return(&cloudpb.GetDownloadLinkResponse{
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
						"url": "http://pixie.com/cli_url",
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
		&cloudpb.GetArtifactListRequest{
			ArtifactName: "cli",
			ArtifactType: cloudpb.AT_LINUX_AMD64,
		}).
		Return(&cloudpb.ArtifactSet{
			Name: "cli",
			Artifact: []*cloudpb.Artifact{{
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
		&cloudpb.GetArtifactListRequest{
			ArtifactName: "vizier",
			ArtifactType: cloudpb.AT_CONTAINER_SET_LINUX_AMD64,
		}).
		Return(&cloudpb.ArtifactSet{
			Name: "vizier",
			Artifact: []*cloudpb.Artifact{{
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

func LoadUnauthenticatedSchema(gqlEnv controller.GraphQLEnv) *graphql.Schema {
	schemaData := noauth.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	qr := &controller.QueryResolver{gqlEnv}
	gqlSchema := graphql.MustParseSchema(schemaData, qr, opts...)
	return gqlSchema
}
func TestArtifacts_Unauthenticated(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := context.Background()

	mockClients.MockArtifact.EXPECT().GetArtifactList(gomock.Any(),
		&cloudpb.GetArtifactListRequest{
			ArtifactName: "vizier",
			ArtifactType: cloudpb.AT_CONTAINER_SET_LINUX_AMD64,
		}).
		Return(&cloudpb.ArtifactSet{
			Name: "vizier",
			Artifact: []*cloudpb.Artifact{{
				VersionStr: "1.2.3",
				Changelog:  "a changelog",
				Timestamp:  &types.Timestamp{Seconds: 10},
			}, {
				VersionStr: "1.2.2",
				Changelog:  "some changes go here",
				Timestamp:  &types.Timestamp{Seconds: 5},
			}},
		}, nil)

	gqlSchema := LoadUnauthenticatedSchema(gqlEnv)
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
