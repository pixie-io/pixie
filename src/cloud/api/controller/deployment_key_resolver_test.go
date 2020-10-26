package controller_test

import (
	"testing"
	"time"

	types "github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils"
)

func TestDeploymentKey(t *testing.T) {
	keyID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	createTime := time.Date(2020, 03, 9, 17, 46, 100, 1232409, time.UTC)
	createTimePb, err := types.TimestampProto(createTime)
	if err != nil {
		t.Fatalf("could not write time %+v as protobuf", createTime)
	}

	mockClients.MockVizierDeployKey.EXPECT().
		Get(gomock.Any(), &cloudapipb.GetDeploymentKeyRequest{
			ID: utils.ProtoFromUUIDStrOrNil(keyID),
		}).
		Return(&cloudapipb.GetDeploymentKeyResponse{
			Key: &cloudapipb.DeploymentKey{
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
					deploymentKey(id: "7ba7b810-9dad-11d1-80b4-00c04fd430c8") {
						id
						key
						createdAtMs
						desc
					}
				}
			`,
			ExpectedResult: `
				{
					"deploymentKey": {
						"id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"key": "foobar",
						"createdAtMs": 1583776060001.2324,
						"desc": "key description"
					}
				}
			`,
		},
	})
}

func TestDeploymentKeys(t *testing.T) {
	key1ID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	key2ID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	key3ID := "8cb848c6-9dad-11d1-80b4-00c04fd430c8"

	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

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
	mockClients.MockVizierDeployKey.EXPECT().
		List(gomock.Any(), &cloudapipb.ListDeploymentKeyRequest{}).
		Return(&cloudapipb.ListDeploymentKeyResponse{
			Keys: []*cloudapipb.DeploymentKey{
				&cloudapipb.DeploymentKey{
					ID:        utils.ProtoFromUUIDStrOrNil(key1ID),
					Key:       "abc",
					CreatedAt: createTime1Pb,
					Desc:      "key description 1",
				},
				&cloudapipb.DeploymentKey{
					ID:        utils.ProtoFromUUIDStrOrNil(key2ID),
					Key:       "def",
					CreatedAt: createTime2Pb,
					Desc:      "key description 2",
				},
				&cloudapipb.DeploymentKey{
					ID:        utils.ProtoFromUUIDStrOrNil(key3ID),
					Key:       "ghi",
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
					deploymentKeys {
						id
						key
						createdAtMs
						desc
					}
				}
			`,
			ExpectedResult: `
				{
					"deploymentKeys": [{
						"id": "8cb848c6-9dad-11d1-80b4-00c04fd430c8",
						"key": "ghi",
						"createdAtMs": 1601747260000.4124,
						"desc": "key description 3"
					}, {
						"id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"key": "abc",
						"createdAtMs": 1583776060001.2324,
						"desc": "key description 1"
					},
					{
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"key": "def",
						"createdAtMs": 1572803260000.4124,
						"desc": "key description 2"
					}]
				}
			`,
		},
	})
}

func TestCreateDeploymentKey(t *testing.T) {
	keyID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	createTime := time.Date(2020, 03, 9, 17, 46, 100, 1232409, time.UTC)
	createTimePb, err := types.TimestampProto(createTime)
	if err != nil {
		t.Fatalf("could not write time %+v as protobuf", createTime)
	}

	mockClients.MockVizierDeployKey.EXPECT().
		Create(gomock.Any(), &cloudapipb.CreateDeploymentKeyRequest{}).
		Return(&cloudapipb.DeploymentKey{
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
					CreateDeploymentKey {
						id
						key
						createdAtMs
						desc
					}
				}
			`,
			ExpectedResult: `
				{
					"CreateDeploymentKey": {
						"id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"key": "foobar",
						"createdAtMs": 1583776060001.2324,
						"desc": "key description"				
					}
				}
			`,
		},
	})
}

func TestDeleteDeploymentKey(t *testing.T) {
	keyID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	gqlEnv, mockClients, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockVizierDeployKey.EXPECT().
		Delete(gomock.Any(), utils.ProtoFromUUIDStrOrNil(keyID)).
		Return(&types.Empty{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					DeleteDeploymentKey(id: "7ba7b810-9dad-11d1-80b4-00c04fd430c8")
				}
			`,
			ExpectedResult: `
				{
					"DeleteDeploymentKey": true
				}
			`,
		},
	})
}
