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
	"time"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controller"
	"px.dev/pixie/src/cloud/api/controller/schema/complete"
	"px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/shared/services/authcontext"
	svcutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = svcutils.GenerateJWTForUser("6ba7b810-9dad-11d1-80b4-00c04fd430c9", "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "test@test.com", time.Now(), "pixie")
	return authcontext.NewContext(context.Background(), sCtx)
}

func LoadSchema(gqlEnv controller.GraphQLEnv) *graphql.Schema {
	schemaData := complete.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	qr := &controller.QueryResolver{gqlEnv}
	gqlSchema := graphql.MustParseSchema(schemaData, qr, opts...)
	return gqlSchema
}

func TestClusterInfo(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	clusterInfo := &cloudpb.ClusterInfo{
		ID:              utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Status:          cloudpb.CS_HEALTHY,
		LastHeartbeatNs: 4 * 1000 * 1000,
		Config: &cloudpb.VizierConfig{
			PassthroughEnabled: false,
		},
		VizierVersion:  "vzVersion",
		ClusterVersion: "clusterVersion",
		ClusterName:    "clusterName",
		ClusterUID:     "clusterUID",
	}

	mockClients.MockVizierClusterInfo.EXPECT().
		GetClusterInfo(gomock.Any(), &cloudpb.GetClusterInfoRequest{
			ID: clusterInfo.ID,
		}).
		Return(&cloudpb.GetClusterInfoResponse{
			Clusters: []*cloudpb.ClusterInfo{clusterInfo},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					cluster(id: "7ba7b810-9dad-11d1-80b4-00c04fd430c8") {
						id
						status
						lastHeartbeatMs
						vizierConfig {
							passthroughEnabled
						}
						vizierVersion
						clusterVersion
						clusterName
						clusterUID
					}
				}
			`,
			ExpectedResult: `
				{
					"cluster": {
						"id":"7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"status": "CS_HEALTHY",
						"lastHeartbeatMs": 4,
						"vizierConfig": {
							"passthroughEnabled": false
						},
						"vizierVersion": "vzVersion",
						"clusterVersion": "clusterVersion",
						"clusterName": "clusterName",
						"clusterUID": "clusterUID"
					}
				}
			`,
		},
	})
}

func TestClusterInfoByName(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	clusterInfo := &cloudpb.ClusterInfo{
		ID:              utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Status:          cloudpb.CS_HEALTHY,
		LastHeartbeatNs: 4 * 1000 * 1000,
		Config: &cloudpb.VizierConfig{
			PassthroughEnabled: false,
		},
		VizierVersion:  "vzVersion",
		ClusterVersion: "clusterVersion",
		ClusterName:    "clusterName",
		ClusterUID:     "clusterUID",
	}

	unmatchedClusterInfo := &cloudpb.ClusterInfo{
		ID:              utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"),
		Status:          cloudpb.CS_HEALTHY,
		LastHeartbeatNs: 4 * 1000 * 1000,
		Config: &cloudpb.VizierConfig{
			PassthroughEnabled: false,
		},
		VizierVersion:  "vzVersion",
		ClusterVersion: "clusterVersion2",
		ClusterName:    "clusterName2",
		ClusterUID:     "clusterUID2",
	}

	mockClients.MockVizierClusterInfo.EXPECT().
		GetClusterInfo(gomock.Any(), &cloudpb.GetClusterInfoRequest{}).
		Return(&cloudpb.GetClusterInfoResponse{
			Clusters: []*cloudpb.ClusterInfo{unmatchedClusterInfo, clusterInfo},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					clusterByName(name: "clusterName") {
						id
						status
						lastHeartbeatMs
						vizierConfig {
							passthroughEnabled
						}
						vizierVersion
						clusterVersion
						clusterName
						clusterUID
					}
				}
			`,
			ExpectedResult: `
				{
					"clusterByName": {
						"id":"7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"status": "CS_HEALTHY",
						"lastHeartbeatMs": 4,
						"vizierConfig": {
							"passthroughEnabled": false
						},
						"vizierVersion": "vzVersion",
						"clusterVersion": "clusterVersion",
						"clusterName": "clusterName",
						"clusterUID": "clusterUID"
					}
				}
			`,
		},
	})
}

func TestClusterConnectionInfo(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	clusterID := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockClients.MockVizierClusterInfo.EXPECT().
		GetClusterConnectionInfo(gomock.Any(), &cloudpb.GetClusterConnectionInfoRequest{
			ID: clusterID,
		}).
		Return(&cloudpb.GetClusterConnectionInfoResponse{
			IPAddress: "127.0.0.1",
			Token:     "this-is-a-token",
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					clusterConnection(id: "7ba7b810-9dad-11d1-80b4-00c04fd430c8") {
						ipAddress
						token
					}
				}
			`,
			ExpectedResult: `
				{
					"clusterConnection": {
						"ipAddress": "127.0.0.1",
						"token": "this-is-a-token"
					}
				}
			`,
		},
	})
}

func TestUpdateClusterVizierConfig(t *testing.T) {
	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8")
	mockClients.MockVizierClusterInfo.EXPECT().
		UpdateClusterVizierConfig(gomock.Any(), &cloudpb.UpdateClusterVizierConfigRequest{
			ID: id,
			ConfigUpdate: &cloudpb.VizierConfigUpdate{
				PassthroughEnabled: &types.BoolValue{Value: true},
			},
		}).
		Return(&cloudpb.UpdateClusterVizierConfigResponse{}, nil)

	clusterInfo := &cloudpb.ClusterInfo{
		ID:              id,
		Status:          cloudpb.CS_HEALTHY,
		LastHeartbeatNs: 4 * 1000 * 1000,
		Config: &cloudpb.VizierConfig{
			PassthroughEnabled: true,
		},
		VizierVersion:  "vzVersion",
		ClusterVersion: "clusterVersion",
		ClusterName:    "clusterName",
		ClusterUID:     "clusterUID",
	}

	mockClients.MockVizierClusterInfo.EXPECT().
		GetClusterInfo(gomock.Any(), &cloudpb.GetClusterInfoRequest{
			ID: clusterInfo.ID,
		}).
		Return(&cloudpb.GetClusterInfoResponse{
			Clusters: []*cloudpb.ClusterInfo{clusterInfo},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateVizierConfig(clusterID: "7ba7b810-9dad-11d1-80b4-00c04fd430c8", vizierConfig: { passthroughEnabled: true }) {
						id
						vizierConfig {
							passthroughEnabled
						}
						lastHeartbeatMs
					}
				}
			`,
			ExpectedResult: `
				{
					"UpdateVizierConfig": {
						"id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"vizierConfig": {
							"passthroughEnabled": true
						},
						"lastHeartbeatMs": 4
					}
				}
			`,
		},
	})
}
