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
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	gqltestutils "px.dev/pixie/src/cloud/api/controllers/testutils"
)

func TestPluginResolver_Plugins(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetPlugins(gomock.Any(), &cloudpb.GetPluginsRequest{
		Kind: cloudpb.PK_RETENTION,
	}).Return(&cloudpb.GetPluginsResponse{
		Plugins: []*cloudpb.Plugin{
			&cloudpb.Plugin{
				Name:               "Test Plugin",
				Id:                 "test-plugin",
				Description:        "Here is a plugin that is used for this test",
				Logo:               "",
				LatestVersion:      "2.0.0",
				RetentionSupported: true,
				RetentionEnabled:   false,
				EnabledVersion:     "",
			},
			&cloudpb.Plugin{
				Name:               "Another Plugin",
				Id:                 "another-plugin",
				Description:        "Here is a another plugin that is used for this test",
				Logo:               "<svg></svg>",
				LatestVersion:      "3.0.0",
				RetentionSupported: true,
				RetentionEnabled:   true,
				EnabledVersion:     "2.0.0",
			},
		},
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					plugins(kind: PK_RETENTION) {
						name
						id
						description
						logo
						latestVersion
						supportsRetention
						retentionEnabled
						enabledVersion
					}
				}
			`,
			ExpectedResult: `
				{
					"plugins": [
						{

							"name": "Test Plugin",
							"id": "test-plugin",
							"description": "Here is a plugin that is used for this test",
							"logo": null,
							"latestVersion": "2.0.0",
							"supportsRetention": true,
							"retentionEnabled": false,
							"enabledVersion": null
						},
						{

							"name": "Another Plugin",
							"id": "another-plugin",
							"description": "Here is a another plugin that is used for this test",
							"logo": "<svg></svg>",
							"latestVersion": "3.0.0",
							"supportsRetention": true,
							"retentionEnabled": true,
							"enabledVersion": "2.0.0"
						}
					]
				}
			`,
		},
	})
}

func TestPluginResolver_OrgRetentionPluginConfig(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetOrgRetentionPluginConfig(gomock.Any(), &cloudpb.GetOrgRetentionPluginConfigRequest{
		PluginId: "test-plugin",
	}).Return(&cloudpb.GetOrgRetentionPluginConfigResponse{
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					orgRetentionPluginConfig(id: "test-plugin") {
						name
						value
					}
				}
			`,
			ExpectedResult: `
				{
					"orgRetentionPluginConfig": [
						{

							"name": "API_KEY",
							"value": "test-api-key"
						}
					]
				}
			`,
		},
	})
}

func TestPluginResolver_RetentionPluginInfo(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().GetRetentionPluginInfo(gomock.Any(), &cloudpb.GetRetentionPluginInfoRequest{
		PluginId: "test-plugin",
		Version:  "2.0.0",
	}).Return(&cloudpb.GetRetentionPluginInfoResponse{
		Configs: map[string]string{
			"API_KEY": "This is an API key used in the product.",
		},
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					retentionPluginInfo(id: "test-plugin", pluginVersion: "2.0.0") {
						configs {
							name
							description
						}
					}
				}
			`,
			ExpectedResult: `
				{
					"retentionPluginInfo": {
						"configs": [
							{
								"name": "API_KEY",
								"description": "This is an API key used in the product."
							}
						]
					}
				}
			`,
		},
	})
}

func TestPluginResolver_UpdateRetentionPluginConfig(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockPlugin.EXPECT().UpdateRetentionPluginConfig(gomock.Any(), &cloudpb.UpdateRetentionPluginConfigRequest{
		Configs: map[string]string{
			"API_KEY": "test-api-key",
		},
		PluginId: "test-plugin",
		Enabled: &types.BoolValue{
			Value: true,
		},
		Version: &types.StringValue{
			Value: "2.0.0",
		},
	}).Return(&cloudpb.UpdateRetentionPluginConfigResponse{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateRetentionPluginConfig(id: "test-plugin", enabled: true, enabledVersion: "2.0.0", configs: {
						configs: [
							{
								name: "API_KEY",
								value: "test-api-key"
							}
						]
					})
				}
			`,
			ExpectedResult: `
				{
					"UpdateRetentionPluginConfig": true
				}
			`,
		},
	})
}
