package controller_test

import (
	"context"
	"sort"
	"testing"
	"time"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/schema"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
	"pixielabs.ai/pixielabs/src/utils"
)

type vizierStatus cloudpb.VizierInfo_Status

func setupTestDB(t *testing.T) (*sqlx.DB, func()) {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})
	db, teardown := pgtest.SetupTestDB(t, s)

	return db, func() {
		teardown()
	}
}

func loadTestData(t *testing.T, db *sqlx.DB) {
	insertVizierClusterQuery := `INSERT INTO vizier_cluster(org_id, id) VALUES ($1, $2)`
	db.MustExec(insertVizierClusterQuery, "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440000")
	db.MustExec(insertVizierClusterQuery, "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440001")
	db.MustExec(insertVizierClusterQuery, "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440002")
	db.MustExec(insertVizierClusterQuery, "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440003")

	insertVizierClusterInfoQuery := `INSERT INTO vizier_cluster_info(vizier_cluster_id, status, address, jwt_signing_key, last_heartbeat) VALUES($1, $2, $3, $4, $5)`
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440000", "UNKNOWN", "addr0", "key0", "2011-05-16 15:36:38")
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440001", "HEALTHY", "addr1", "key1", "2011-05-17 15:36:38")
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440002", "UNHEALTHY", "addr2", "key2", "2011-05-18 15:36:38")
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440003", "DISCONNECTED", "addr3", "key3", "2011-05-19 15:36:38")
}

func TestServer_CreateVizierCluster(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db)
	orgID := uuid.NewV4()

	tests := []struct {
		name string
		org  *uuidpb.UUID
		// Expectations
		hasError bool
		errCode  codes.Code
	}{
		{
			name:     "invalid input org id",
			org:      nil,
			hasError: true,
			errCode:  codes.InvalidArgument,
		},
		{
			name:     "nil input org id",
			org:      utils.ProtoFromUUIDStrOrNil("abc"),
			hasError: true,
			errCode:  codes.InvalidArgument,
		},
		{
			name:     "valid request",
			org:      utils.ProtoFromUUID(&orgID),
			hasError: false,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			resp, err := s.CreateVizierCluster(context.Background(), &vzmgrpb.CreateVizierClusterRequest{OrgID: tc.org})
			if tc.hasError {
				assert.NotNil(t, err)
				assert.Nil(t, resp)
				assert.Equal(t, status.Code(err), tc.errCode)
			} else {
				assert.Nil(t, err)
				assert.NotNil(t, resp)
				assert.NotEqual(t, utils.UUIDFromProtoOrNil(resp), uuid.Nil)

				// Check to make sure DB insert is correct.
				query := `SELECT id, org_id from vizier_cluster WHERE id=$1`
				var info struct {
					ID    uuid.UUID `db:"id"`
					OrgID uuid.UUID `db:"org_id"`
				}
				err := db.Get(&info, query, utils.UUIDFromProtoOrNil(resp))
				require.Nil(t, err)
				assert.Equal(t, info.OrgID, utils.UUIDFromProtoOrNil(tc.org))
				assert.Equal(t, info.ID, utils.UUIDFromProtoOrNil(resp))

				// Check to make sure DB insert for ClusterInfo is correct.
				connQuery := `SELECT vizier_cluster_id, status from vizier_cluster_info WHERE vizier_cluster_id=$1`
				var connInfo struct {
					ID     uuid.UUID    `db:"vizier_cluster_id"`
					Status vizierStatus `db:"status"`
				}
				err = db.Get(&connInfo, connQuery, utils.UUIDFromProtoOrNil(resp))
				assert.Equal(t, connInfo.ID, utils.UUIDFromProtoOrNil(resp))
			}
		})
	}
}

func TestServer_GetViziersByOrg(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db)

	t.Run("valid", func(t *testing.T) {
		// Fetch the test data that was inserted earlier.
		resp, err := s.GetViziersByOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"))
		require.Nil(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, len(resp.VizierIDs), 4)

		// Convert to UUID string array.
		var ids []string
		for _, val := range resp.VizierIDs {
			ids = append(ids, utils.UUIDFromProtoOrNil(val).String())
		}
		sort.Strings(ids)
		expected := []string{
			"123e4567-e89b-12d3-a456-426655440000",
			"123e4567-e89b-12d3-a456-426655440001",
			"123e4567-e89b-12d3-a456-426655440002",
			"123e4567-e89b-12d3-a456-426655440003",
		}
		sort.Strings(expected)
		assert.Equal(t, ids, expected)
	})

	t.Run("No such org id", func(t *testing.T) {
		resp, err := s.GetViziersByOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"))
		require.Nil(t, err)
		require.NotNil(t, resp)
		assert.Equal(t, len(resp.VizierIDs), 0)
	})

	t.Run("bad input org id", func(t *testing.T) {
		resp, err := s.GetViziersByOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("3e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		assert.Nil(t, resp)
		assert.Equal(t, status.Code(err), codes.InvalidArgument)
	})

	t.Run("missing input org id", func(t *testing.T) {
		resp, err := s.GetViziersByOrg(context.Background(), nil)
		require.NotNil(t, err)
		assert.Nil(t, resp)
		assert.Equal(t, status.Code(err), codes.InvalidArgument)
	})
}

func TestServer_GetVizierInfo(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db)
	resp, err := s.GetVizierInfo(context.Background(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.Nil(t, err)
	require.NotNil(t, resp)

	// TODO(zasgar): write more tests here.
}

func TestServer_GetVizierConnectionInfo(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db)
	resp, err := s.GetVizierConnectionInfo(context.Background(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, resp.IPAddress, "addr1")
	assert.NotNil(t, resp.Token)

	// TODO(zasgar): write more tests here.
}

func TestServer_VizierConnected(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db)
	req := &cloudpb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		JwtKey:   "the-token",
	}

	resp, err := s.VizierConnected(context.Background(), req)
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, resp.Status, cloudpb.ST_OK)

	// TODO(zasgar): write more tests here.
}

func TestServer_HandleVizierHeartbeat(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db)

	t.Run("valid Vizier", func(t *testing.T) {
		req := &cloudpb.VizierHeartbeat{
			VizierID:       utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
			Time:           100,
			SequenceNumber: 200,
		}
		resp, err := s.HandleVizierHeartbeat(context.Background(), req)
		require.Nil(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.SequenceNumber, req.SequenceNumber)
		assert.True(t, resp.Time >= time.Now().Unix())
		assert.Equal(t, resp.Status, cloudpb.HB_OK)
	})

	t.Run("unknown Vizier", func(t *testing.T) {
		req := &cloudpb.VizierHeartbeat{
			VizierID:       utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
			Time:           100,
			SequenceNumber: 200,
		}
		resp, err := s.HandleVizierHeartbeat(context.Background(), req)
		require.NotNil(t, err)
		require.Nil(t, resp)

		assert.Equal(t, status.Code(err), codes.NotFound)
	})

	// TODO(zasgar): Add more tests here.
}
