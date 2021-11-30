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
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/utils"
)

func TestAPIKey(t *testing.T) {
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
			keyID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			createTime := time.Date(2020, 03, 9, 17, 46, 100, 1232409, time.UTC)
			createTimePb, err := types.TimestampProto(createTime)
			if err != nil {
				t.Fatalf("could not write time %+v as protobuf", createTime)
			}

			mockClients.MockAPIKey.EXPECT().
				Get(gomock.Any(), &cloudpb.GetAPIKeyRequest{
					ID: utils.ProtoFromUUIDStrOrNil(keyID),
				}).
				Return(&cloudpb.GetAPIKeyResponse{
					Key: &cloudpb.APIKey{
						ID:        utils.ProtoFromUUIDStrOrNil(keyID),
						Key:       "foobar",
						CreatedAt: createTimePb,
						Desc:      "key description",
					},
				}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
					 query {
						 apiKey(id: "7ba7b810-9dad-11d1-80b4-00c04fd430c8") {
							 id
							 key
							 createdAtMs
							 desc
						 }
					 }
				 `,
					ExpectedResult: `
					 {
						 "apiKey": {
							 "id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
							 "key": "foobar",
							 "createdAtMs": 1583776060001.2324,
							 "desc": "key description"
						 }
					 }
				 `,
				},
			})
		})
	}
}

func TestAPIKeys(t *testing.T) {
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
			key1ID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
			key2ID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
			key3ID := "8cb848c6-9dad-11d1-80b4-00c04fd430c8"

			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			createTime1 := time.Date(2020, 03, 9, 17, 46, 100, 1232409, time.UTC)
			createTime1Pb, err := types.TimestampProto(createTime1)
			if err != nil {
				t.Fatalf("could not write time %+v as protobuf", createTime1)
			}
			createTime2 := time.Date(2019, 11, 3, 17, 46, 100, 412401, time.UTC)
			createTime2Pb, err := types.TimestampProto(createTime2)
			if err != nil {
				t.Fatalf("could not write time %+v as protobuf", createTime2)
			}
			createTime3 := time.Date(2020, 10, 3, 17, 46, 100, 412401, time.UTC)
			createTime3Pb, err := types.TimestampProto(createTime3)
			if err != nil {
				t.Fatalf("could not write time %+v as protobuf", createTime3)
			}

			// Inserted keys are not sorted by creation time.
			mockClients.MockAPIKey.EXPECT().
				List(gomock.Any(), &cloudpb.ListAPIKeyRequest{}).
				Return(&cloudpb.ListAPIKeyResponse{
					Keys: []*cloudpb.APIKeyMetadata{
						{
							ID:        utils.ProtoFromUUIDStrOrNil(key1ID),
							CreatedAt: createTime1Pb,
							Desc:      "key description 1",
						},
						{
							ID:        utils.ProtoFromUUIDStrOrNil(key2ID),
							CreatedAt: createTime2Pb,
							Desc:      "key description 2",
						},
						{
							ID:        utils.ProtoFromUUIDStrOrNil(key3ID),
							CreatedAt: createTime3Pb,
							Desc:      "key description 3",
						},
					},
				}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			// Expect returned keys to be sorted.
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						query {
							apiKeys {
								id
								createdAtMs
								desc
							}
						}
					`,
					ExpectedResult: `
						{
							"apiKeys": [{
								"id": "8cb848c6-9dad-11d1-80b4-00c04fd430c8",
								"createdAtMs": 1601747260000.4124,
								"desc": "key description 3"
							}, {
								"id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
								"createdAtMs": 1583776060001.2324,
								"desc": "key description 1"
							},
							{
								"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
								"createdAtMs": 1572803260000.4124,
								"desc": "key description 2"
							}]
						}
					`,
				},
			})
		})
	}
}

func TestCreateAPIKey(t *testing.T) {
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
			keyID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			createTime := time.Date(2020, 03, 9, 17, 46, 100, 1232409, time.UTC)
			createTimePb, err := types.TimestampProto(createTime)
			if err != nil {
				t.Fatalf("could not write time %+v as protobuf", createTime)
			}

			mockClients.MockAPIKey.EXPECT().
				Create(gomock.Any(), &cloudpb.CreateAPIKeyRequest{}).
				Return(&cloudpb.APIKey{
					ID:        utils.ProtoFromUUIDStrOrNil(keyID),
					Key:       "foobar",
					CreatedAt: createTimePb,
					Desc:      "key description",
				}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
					mutation {
						CreateAPIKey {
							id
							createdAtMs
							desc
						}
					}
				`,
					ExpectedResult: `
					{
						"CreateAPIKey": {
							"id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
							"createdAtMs": 1583776060001.2324,
							"desc": "key description"
						}
					}
				`,
				},
			})
		})
	}
}

func TestDeleteAPIKey(t *testing.T) {
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
			keyID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

			gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockAPIKey.EXPECT().
				Delete(gomock.Any(), utils.ProtoFromUUIDStrOrNil(keyID)).
				Return(&types.Empty{}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						mutation {
							DeleteAPIKey(id: "7ba7b810-9dad-11d1-80b4-00c04fd430c8")
						}
					`,
					ExpectedResult: `
						{
							"DeleteAPIKey": true
						}
					`,
				},
			})
		})
	}
}
