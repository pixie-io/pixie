package deploymentkey

import (
	"context"
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/cloud/vzmgr/schema"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzerrors"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
	jwtutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils"
)

var (
	testAuthOrgID    = uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")
	testAuthUserID   = uuid.FromStringOrNil("423e4567-e89b-12d3-a456-426655440000")
	testNonAuthOrgID = uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440001")

	testKey1ID           = uuid.FromStringOrNil("883e4567-e89b-12d3-a456-426655440000")
	testKey2ID           = uuid.FromStringOrNil("993e4567-e89b-12d3-a456-426655440000")
	testNonAuthUserKeyID = uuid.FromStringOrNil("003e4567-e89b-12d3-a456-426655440000")
	testDBKey            = "test_db_key"
)

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

var db *sqlx.DB

func testMain(m *testing.M) error {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	m.Run()
	return nil
}

func createTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = jwtutils.GenerateJWTForUser(testAuthUserID.String(), testAuthOrgID.String(), "test@test.com", time.Now())
	return authcontext.NewContext(context.Background(), sCtx)
}

func mustLoadTestData(db *sqlx.DB) {
	db.MustExec(`DELETE FROM vizier_deployment_keys`)

	insertVizierDeploymentKeys := `INSERT INTO vizier_deployment_keys(id, org_id, user_id, key, description) VALUES ($1, $2, $3, PGP_SYM_ENCRYPT($4, $5), $6)`
	db.MustExec(insertVizierDeploymentKeys, testKey1ID, testAuthOrgID, testAuthUserID, "key1", testDBKey, "here is a desc")
	db.MustExec(insertVizierDeploymentKeys, testKey2ID, testAuthOrgID, testAuthUserID, "key2", testDBKey, "here is another one")
	db.MustExec(insertVizierDeploymentKeys, testNonAuthUserKeyID.String(), testNonAuthOrgID, "123e4567-e89b-12d3-a456-426655440001", "key2", testDBKey, "some other desc")
}

func TestDeploymentKeyService_CreateDeploymentKey(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)
	resp, err := svc.Create(ctx, &vzmgrpb.CreateDeploymentKeyRequest{Desc: "this is a key"})
	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Check if time is reasonable.
	ts, err := types.TimestampFromProto(resp.CreatedAt)
	assert.Nil(t, err)

	diff := time.Since(ts).Milliseconds()
	if diff < 0 {
		diff = -1 * diff
	}
	assert.LessOrEqual(t, diff, int64(10000))

	// Check if the key has a value and the ID looks valid.
	assert.Greater(t, len(resp.Key), 0)
	assert.NotEqual(t, uuid.Nil.String(), utils.UUIDFromProtoOrNil(resp.ID).String())
}

func TestDeploymentKeyService_ListDeploymentKeys(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)
	resp, err := svc.List(ctx, &vzmgrpb.ListDeploymentKeyRequest{})
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, 2, len(resp.Keys))
	assert.Equal(t, testKey1ID, utils.UUIDFromProtoOrNil(resp.Keys[0].ID))
	assert.Equal(t, testKey2ID, utils.UUIDFromProtoOrNil(resp.Keys[1].ID))
	assert.Equal(t, "here is a desc", resp.Keys[0].Desc)
	assert.Equal(t, "here is another one", resp.Keys[1].Desc)
	assert.Equal(t, "key1", resp.Keys[0].Key)
	assert.Equal(t, "key2", resp.Keys[1].Key)

	// Check that time looks reasonable.
	ts, err := types.TimestampFromProto(resp.Keys[0].CreatedAt)
	assert.Nil(t, err)

	diff := time.Since(ts).Milliseconds()
	if diff < 0 {
		diff = -1 * diff
	}
	assert.LessOrEqual(t, diff, int64(10000))
}

func TestDeploymentKeyService_ListDeploymentKeys_MissingAuth(t *testing.T) {
	mustLoadTestData(db)

	ctx := context.Background()
	svc := New(db, testDBKey)
	resp, err := svc.List(ctx, &vzmgrpb.ListDeploymentKeyRequest{})
	assert.Nil(t, resp)
	assert.NotNil(t, err)

	assert.Equal(t, codes.Unauthenticated, status.Code(err))
}

func TestDeploymentKeyService_Get(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	resp, err := svc.Get(ctx, &vzmgrpb.GetDeploymentKeyRequest{
		ID: utils.ProtoFromUUID(testKey1ID),
	})

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	assert.Equal(t, "key1", resp.Key.Key)
	// Check if time is reasonable.
	ts, err := types.TimestampFromProto(resp.Key.CreatedAt)
	assert.Nil(t, err)

	diff := time.Since(ts).Milliseconds()
	if diff < 0 {
		diff = -1 * diff
	}
	assert.LessOrEqual(t, diff, int64(10000))
	assert.Equal(t, "here is a desc", resp.Key.Desc)
}

func TestDeploymentKeyService_Get_UnownedID(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	resp, err := svc.Get(ctx, &vzmgrpb.GetDeploymentKeyRequest{
		ID: utils.ProtoFromUUID(testNonAuthUserKeyID),
	})
	assert.Nil(t, resp)
	assert.NotNil(t, err)

	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestDeploymentKeyService_Get_NonExistentID(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	u := uuid.Must(uuid.NewV4())
	resp, err := svc.Get(ctx, &vzmgrpb.GetDeploymentKeyRequest{
		ID: utils.ProtoFromUUID(u),
	})
	assert.Nil(t, resp)
	assert.NotNil(t, err)

	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestDeploymentKeyService_Delete(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	u := utils.ProtoFromUUID(testKey1ID)
	resp, err := svc.Delete(ctx, u)
	assert.Nil(t, err)
	assert.NotNil(t, resp)

	_, err = svc.Get(ctx, &vzmgrpb.GetDeploymentKeyRequest{
		ID: u,
	})
	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestDeploymentKeyService_Delete_UnownedKey(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	u := utils.ProtoFromUUID(testNonAuthUserKeyID)
	resp, err := svc.Delete(ctx, u)
	assert.NotNil(t, err)
	assert.Nil(t, resp)
	assert.Equal(t, codes.NotFound, status.Code(err))

	// Make DB query to make sure the Key still exists.
	var key string
	err = db.QueryRow(`SELECT PGP_SYM_DECRYPT(key::bytea, $1) from vizier_deployment_keys where id=$2`,
		testDBKey, testNonAuthUserKeyID).
		Scan(&key)
	assert.Nil(t, err)
	assert.Equal(t, "key2", key)
}

func TestDeploymentKeyService_Delete_NonExistentKey(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	u := uuid.Must(uuid.NewV4())
	resp, err := svc.Delete(ctx, utils.ProtoFromUUID(u))
	assert.NotNil(t, err)
	assert.Nil(t, resp)
	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestService_FetchOrgUserIDUsingDeploymentKey(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	orgID, userID, err := svc.FetchOrgUserIDUsingDeploymentKey(ctx, "key1")
	assert.Nil(t, err)
	assert.Equal(t, testAuthOrgID, orgID)
	assert.Equal(t, testAuthUserID, userID)
}

func TestService_FetchOrgUserIDUsingDeploymentKey_BadKey(t *testing.T) {
	mustLoadTestData(db)

	ctx := createTestContext()
	svc := New(db, testDBKey)

	orgID, userID, err := svc.FetchOrgUserIDUsingDeploymentKey(ctx, "some rando key that does not exist")
	assert.NotNil(t, err)
	assert.Equal(t, vzerrors.ErrDeploymentKeyNotFound, err)
	assert.Equal(t, uuid.Nil, orgID)
	assert.Equal(t, uuid.Nil, userID)
}
