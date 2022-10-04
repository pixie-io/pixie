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
	"time"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/schema/complete"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/shared/services/authcontext"
	svcutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = svcutils.GenerateJWTForUser("6ba7b810-9dad-11d1-80b4-00c04fd430c9", "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "test@test.com", time.Now(), "pixie")
	return authcontext.NewContext(context.Background(), sCtx)
}

func CreateTestContextNoOrg() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = svcutils.GenerateJWTForUser("6ba7b810-9dad-11d1-80b4-00c04fd430c9", "00000000-0000-0000-0000-000000000000", "email-password@fancy.com", time.Now(), "pixie")
	return authcontext.NewContext(context.Background(), sCtx)
}

func CreateAPIUserTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = svcutils.GenerateJWTForAPIUser("6ba7b810-9dad-11d1-80b4-00c04fd430c9", "6ba7b810-9dad-11d1-80b4-00c04fd430c8", time.Now(), "pixie")
	return authcontext.NewContext(context.Background(), sCtx)
}

func LoadSchema(gqlEnv controllers.GraphQLEnv) *graphql.Schema {
	schemaData := complete.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	qr := &controllers.QueryResolver{gqlEnv}
	gqlSchema := graphql.MustParseSchema(schemaData, qr, opts...)
	return gqlSchema
}

func TestClusterInfo(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx
			ctrlPlane := make(map[string]*cloudpb.PodStatus)
			ctrlPlane["vizier-foo"] = &cloudpb.PodStatus{
				Name:         "vizier-foo",
				Status:       cloudpb.RUNNING,
				RestartCount: 10,
			}

			clusterInfo := &cloudpb.ClusterInfo{
				ID:                      utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Status:                  cloudpb.CS_HEALTHY,
				LastHeartbeatNs:         4 * 1000 * 1000,
				VizierVersion:           "vzVersion",
				OperatorVersion:         "opVersion",
				ClusterVersion:          "clusterVersion",
				ClusterName:             "clusterName",
				ClusterUID:              "clusterUID",
				StatusMessage:           "Everything is running",
				PreviousStatus:          cloudpb.CS_UNHEALTHY,
				PreviousStatusTime:      types.TimestampNow(),
				ControlPlanePodStatuses: ctrlPlane,
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
								controlPlanePodStatuses {
									name
									status
									restartCount
								}
								vizierVersion
								operatorVersion
								clusterVersion
								clusterName
								clusterUID
								statusMessage
								previousStatus
							}
						}
					`,
					ExpectedResult: `
						{
							"cluster": {
								"id":"7ba7b810-9dad-11d1-80b4-00c04fd430c8",
								"status": "CS_HEALTHY",
								"lastHeartbeatMs": 4,
								"controlPlanePodStatuses": [{
									"name": "vizier-foo",
									"status": "RUNNING",
									"restartCount": 10
								}],
								"vizierVersion": "vzVersion",
								"operatorVersion": "opVersion",
								"clusterVersion": "clusterVersion",
								"clusterName": "clusterName",
								"clusterUID": "clusterUID",
								"statusMessage": "Everything is running",
								"previousStatus": "CS_UNHEALTHY"
							}
						}
					`,
				},
			})
		})
	}
}

func TestClusterInfoByName(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			clusterInfo := &cloudpb.ClusterInfo{
				ID:              utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Status:          cloudpb.CS_HEALTHY,
				LastHeartbeatNs: 4 * 1000 * 1000,
				VizierVersion:   "vzVersion",
				ClusterVersion:  "clusterVersion",
				ClusterName:     "clusterName",
				ClusterUID:      "clusterUID",
				StatusMessage:   "Everything is running",
			}

			unmatchedClusterInfo := &cloudpb.ClusterInfo{
				ID:              utils.ProtoFromUUIDStrOrNil("7ba7b810-9dad-11d1-80b4-00c04fd430c9"),
				Status:          cloudpb.CS_HEALTHY,
				LastHeartbeatNs: 4 * 1000 * 1000,
				VizierVersion:   "vzVersion",
				OperatorVersion: "opVersion",
				ClusterVersion:  "clusterVersion2",
				ClusterName:     "clusterName2",
				ClusterUID:      "clusterUID2",
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
								vizierVersion
								operatorVersion
								clusterVersion
								clusterName
								clusterUID
								statusMessage
							}
						}
					`,
					ExpectedResult: `
						{
							"clusterByName": {
								"id":"7ba7b810-9dad-11d1-80b4-00c04fd430c8",
								"status": "CS_HEALTHY",
								"lastHeartbeatMs": 4,
								"vizierVersion": "vzVersion",
								"operatorVersion": "",
								"clusterVersion": "clusterVersion",
								"clusterName": "clusterName",
								"clusterUID": "clusterUID",
								"statusMessage": "Everything is running"
							}
						}
					`,
				},
			})
		})
	}
}
