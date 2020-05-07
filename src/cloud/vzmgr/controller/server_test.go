package controller_test

import (
	"context"
	"errors"
	"sort"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	mock_dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb/mock"
	messagespb "pixielabs.ai/pixielabs/src/cloud/shared/messagespb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/schema"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
	jwtutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

type vizierStatus cvmsgspb.VizierStatus

func setupTestDB(t *testing.T) (*sqlx.DB, func()) {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})
	db, teardown := pgtest.SetupTestDB(t, s)

	return db, func() {
		teardown()
	}
}

var testAuthOrgID = "223e4567-e89b-12d3-a456-426655440000"
var testNonAuthOrgID = "223e4567-e89b-12d3-a456-426655440001"
var testProjectName = "foo"

func loadTestData(t *testing.T, db *sqlx.DB) {
	insertVizierClusterQuery := `INSERT INTO vizier_cluster(org_id, id, project_name) VALUES ($1, $2, $3)`
	db.MustExec(insertVizierClusterQuery, testAuthOrgID, "123e4567-e89b-12d3-a456-426655440000", testProjectName)
	db.MustExec(insertVizierClusterQuery, testAuthOrgID, "123e4567-e89b-12d3-a456-426655440001", testProjectName)
	db.MustExec(insertVizierClusterQuery, testAuthOrgID, "123e4567-e89b-12d3-a456-426655440002", testProjectName)
	db.MustExec(insertVizierClusterQuery, testAuthOrgID, "123e4567-e89b-12d3-a456-426655440003", testProjectName)
	db.MustExec(insertVizierClusterQuery, testNonAuthOrgID, "223e4567-e89b-12d3-a456-426655440003", testProjectName)
	db.MustExec(insertVizierClusterQuery, testNonAuthOrgID, "323e4567-e89b-12d3-a456-426655440003", testProjectName)

	insertVizierClusterInfoQuery := `INSERT INTO vizier_cluster_info(vizier_cluster_id, status, address, jwt_signing_key, last_heartbeat, passthrough_enabled, vizier_version, cluster_version, cluster_uid) VALUES($1, $2, $3, $4, $5, $6, $7, $8, $9)`
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440000", "UNKNOWN", "addr0", "key0", "2011-05-16 15:36:38", true, "", "", "")
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440001", "HEALTHY", "addr1", "\\xc30d04070302c5374a5098262b6d7bd23f01822f741dbebaa680b922b55fd16eb985aeb09505f8fc4a36f0e11ebb8e18f01f684146c761e2234a81e50c21bca2907ea37736f2d9a5834997f4dd9e288c", "2011-05-17 15:36:38", false, "vzVers", "cVers", "cUID")
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440002", "UNHEALTHY", "addr2", "key2", "2011-05-18 15:36:38", true, "", "", "")
	db.MustExec(insertVizierClusterInfoQuery, "123e4567-e89b-12d3-a456-426655440003", "DISCONNECTED", "addr3", "key3", "2011-05-19 15:36:38", false, "", "", "")
	db.MustExec(insertVizierClusterInfoQuery, "223e4567-e89b-12d3-a456-426655440003", "HEALTHY", "addr3", "key3", "2011-05-19 15:36:38", true, "", "", "")
	db.MustExec(insertVizierClusterInfoQuery, "323e4567-e89b-12d3-a456-426655440003", "HEALTHY", "addr3", "key3", "2011-05-19 15:36:38", false, "", "", "")

	insertVizierIndexQuery := `INSERT INTO vizier_index_state(cluster_id, resource_version) VALUES($1, $2)`
	db.MustExec(insertVizierIndexQuery, "123e4567-e89b-12d3-a456-426655440001", "1234")
}

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = jwtutils.GenerateJWTForUser("abcdef", testAuthOrgID, "test@test.com", time.Now())
	return authcontext.NewContext(context.Background(), sCtx)
}

func TestServer_CreateVizierCluster(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)

	tests := []struct {
		name        string
		org         *uuidpb.UUID
		projectName string
		// Expectations
		hasError            bool
		errCode             codes.Code
		expectedProjectName string
	}{
		{
			name:                "valid request",
			org:                 utils.ProtoFromUUIDStrOrNil(testAuthOrgID),
			projectName:         "foo",
			hasError:            false,
			expectedProjectName: "foo",
		},
		{
			name:                "valid request (without project name)",
			org:                 utils.ProtoFromUUIDStrOrNil(testAuthOrgID),
			hasError:            false,
			expectedProjectName: controller.DefaultProjectName,
		},
		{
			name:        "invalid input org id",
			org:         nil,
			projectName: "foo",
			hasError:    true,
			errCode:     codes.InvalidArgument,
		},
		{
			name:        "nil input org id",
			org:         utils.ProtoFromUUIDStrOrNil("abc"),
			projectName: "foo",
			hasError:    true,
			errCode:     codes.InvalidArgument,
		},
		{
			name:        "unauthenticated org id",
			org:         utils.ProtoFromUUIDStrOrNil(testNonAuthOrgID),
			projectName: "foo",
			hasError:    true,
			errCode:     codes.PermissionDenied,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			req := &vzmgrpb.CreateVizierClusterRequest{OrgID: tc.org}
			if tc.projectName != "" {
				req.ProjectName = tc.projectName
			}
			resp, err := s.CreateVizierCluster(CreateTestContext(), req)
			if tc.hasError {
				assert.NotNil(t, err)
				assert.Nil(t, resp)
				assert.Equal(t, status.Code(err), tc.errCode)
			} else {
				assert.Nil(t, err)
				assert.NotNil(t, resp)
				assert.NotEqual(t, utils.UUIDFromProtoOrNil(resp), uuid.Nil)

				// Check to make sure DB insert is correct.
				query := `SELECT id, org_id, project_name from vizier_cluster WHERE id=$1`
				var info struct {
					ID          uuid.UUID `db:"id"`
					OrgID       uuid.UUID `db:"org_id"`
					ProjectName string    `db:"project_name"`
				}
				err := db.Get(&info, query, utils.UUIDFromProtoOrNil(resp))
				require.Nil(t, err)
				assert.Equal(t, info.OrgID, utils.UUIDFromProtoOrNil(tc.org))
				assert.Equal(t, info.ID, utils.UUIDFromProtoOrNil(resp))
				assert.Equal(t, info.ProjectName, tc.expectedProjectName)

				// Check to make sure DB insert for ClusterInfo is correct.
				connQuery := `SELECT vizier_cluster_id, status from vizier_cluster_info WHERE vizier_cluster_id=$1`
				var connInfo struct {
					ID     uuid.UUID    `db:"vizier_cluster_id"`
					Status vizierStatus `db:"status"`
				}
				err = db.Get(&connInfo, connQuery, utils.UUIDFromProtoOrNil(resp))
				assert.Equal(t, connInfo.ID, utils.UUIDFromProtoOrNil(resp))

				// Check to make sure index state is correct.
				idxQuery := `SELECT cluster_id, resource_version from vizier_index_state WHERE cluster_id=$1`
				var idxState struct {
					ID              uuid.UUID `db:"cluster_id"`
					ResourceVersion string    `db:"resource_version"`
				}
				err = db.Get(&idxState, idxQuery, utils.UUIDFromProtoOrNil(resp))
				assert.Equal(t, idxState.ID, utils.UUIDFromProtoOrNil(resp))
				assert.Equal(t, "", idxState.ResourceVersion)
			}
		})
	}
}

func TestServer_GetViziersByOrg(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)

	t.Run("valid", func(t *testing.T) {
		// Fetch the test data that was inserted earlier.
		resp, err := s.GetViziersByOrg(CreateTestContext(), utils.ProtoFromUUIDStrOrNil(testAuthOrgID))
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
		resp, err := s.GetViziersByOrg(CreateTestContext(), utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		assert.Nil(t, resp)
		assert.Equal(t, status.Code(err), codes.PermissionDenied)
	})

	t.Run("bad input org id", func(t *testing.T) {
		resp, err := s.GetViziersByOrg(CreateTestContext(), utils.ProtoFromUUIDStrOrNil("3e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		assert.Nil(t, resp)
		assert.Equal(t, status.Code(err), codes.InvalidArgument)
	})

	t.Run("missing input org id", func(t *testing.T) {
		resp, err := s.GetViziersByOrg(CreateTestContext(), nil)
		require.NotNil(t, err)
		assert.Nil(t, resp)
		assert.Equal(t, status.Code(err), codes.InvalidArgument)
	})

	t.Run("mismatched input org id", func(t *testing.T) {
		resp, err := s.GetViziersByOrg(CreateTestContext(), utils.ProtoFromUUIDStrOrNil(testNonAuthOrgID))
		require.NotNil(t, err)
		assert.Nil(t, resp)
		assert.Equal(t, status.Code(err), codes.PermissionDenied)
	})
}

func TestServer_GetVizierInfo(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)
	resp, err := s.GetVizierInfo(CreateTestContext(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, resp.VizierID, utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	assert.Equal(t, resp.Status, cvmsgspb.VZ_ST_HEALTHY)
	assert.Greater(t, resp.LastHeartbeatNs, int64(0))
	assert.Equal(t, resp.Config.PassthroughEnabled, false)
	assert.Equal(t, "vzVers", resp.VizierVersion)
	assert.Equal(t, "cVers", resp.ClusterVersion)
	assert.Equal(t, "", resp.ClusterName)
	assert.Equal(t, "cUID", resp.ClusterUID)
}

func TestServer_UpdateVizierConfig(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)
	resp, err := s.UpdateVizierConfig(CreateTestContext(), &cvmsgspb.UpdateVizierConfigRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{
			PassthroughEnabled: &types.BoolValue{Value: true},
		},
	})
	require.Nil(t, err)
	require.NotNil(t, resp)

	// Check that the value was actually updated.
	infoResp, err := s.GetVizierInfo(CreateTestContext(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.Nil(t, err)
	require.NotNil(t, infoResp)
	assert.Equal(t, infoResp.Config.PassthroughEnabled, true)
}

func TestServer_UpdateVizierConfig_WrongOrg(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)
	resp, err := s.UpdateVizierConfig(CreateTestContext(), &cvmsgspb.UpdateVizierConfigRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{
			PassthroughEnabled: &types.BoolValue{Value: true},
		},
	})
	require.Nil(t, resp)
	require.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_UpdateVizierConfig_NoUpdates(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)
	resp, err := s.UpdateVizierConfig(CreateTestContext(), &cvmsgspb.UpdateVizierConfigRequest{
		VizierID:     utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{},
	})
	require.Nil(t, err)
	require.NotNil(t, resp)

	// Check that the value was not updated.
	infoResp, err := s.GetVizierInfo(CreateTestContext(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.Nil(t, err)
	require.NotNil(t, infoResp)
	assert.Equal(t, infoResp.Config.PassthroughEnabled, false)
}

func TestServer_GetVizierConnectionInfo(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nil)
	resp, err := s.GetVizierConnectionInfo(CreateTestContext(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"))
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, resp.IPAddress, "https://addr1")
	assert.NotNil(t, resp.Token)
	claims := jwt.MapClaims{}
	_, err = jwt.ParseWithClaims(resp.Token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte("key0"), nil
	})
	assert.Nil(t, err)
	assert.Equal(t, "cluster", claims["Scopes"].(string))

	// TODO(zasgar): write more tests here.
}

func TestServer_VizierConnectedUnhealthy(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()
	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nc)
	req := &cvmsgspb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		JwtKey:   "the-token",
	}

	resp, err := s.VizierConnected(context.Background(), req)
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, resp.Status, cvmsgspb.ST_OK)

	// Check to make sure DB insert for JWT signing key is correct.
	clusterQuery := `SELECT PGP_SYM_DECRYPT(jwt_signing_key::bytea, 'test') as jwt_signing_key, status from vizier_cluster_info WHERE vizier_cluster_id=$1`

	var clusterInfo struct {
		JWTSigningKey string `db:"jwt_signing_key"`
		Status        string `db:"status"`
	}
	clusterID, err := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")
	assert.Nil(t, err)
	err = db.Get(&clusterInfo, clusterQuery, clusterID)
	assert.Nil(t, err)
	assert.Equal(t, "the-token", clusterInfo.JWTSigningKey[controller.SaltLength:])

	assert.Equal(t, "UNHEALTHY", clusterInfo.Status)
	// TODO(zasgar): write more tests here.
}

func TestServer_VizierConnectedHealthy(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()
	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	subCh := make(chan *nats.Msg, 1)
	natsSub, err := nc.ChanSubscribe("VizierConnected", subCh)
	defer natsSub.Unsubscribe()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	s := controller.New(db, "test", mockDNSClient, nc)
	req := &cvmsgspb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		JwtKey:   "the-token",
		Address:  "127.0.0.1",
		ClusterInfo: &cvmsgspb.VizierClusterInfo{
			ClusterUID:     "cluster-uid",
			ClusterName:    "some cluster",
			ClusterVersion: "1234",
			VizierVersion:  "some version",
		},
	}

	resp, err := s.VizierConnected(context.Background(), req)
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, resp.Status, cvmsgspb.ST_OK)

	// Check to make sure DB insert for JWT signing key is correct.
	clusterQuery := `SELECT PGP_SYM_DECRYPT(jwt_signing_key::bytea, 'test') as jwt_signing_key, status, cluster_uid, cluster_name, cluster_version, vizier_version from vizier_cluster_info WHERE vizier_cluster_id=$1`

	var clusterInfo struct {
		JWTSigningKey  string `db:"jwt_signing_key"`
		Status         string `db:"status"`
		ClusterUID     string `db:"cluster_uid"`
		ClusterName    string `db:"cluster_name"`
		ClusterVersion string `db:"cluster_version"`
		VizierVersion  string `db:"vizier_version"`
	}
	clusterID, err := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")
	assert.Nil(t, err)
	err = db.Get(&clusterInfo, clusterQuery, clusterID)
	assert.Nil(t, err)
	assert.Equal(t, "CONNECTED", clusterInfo.Status)
	assert.Equal(t, "cluster-uid", clusterInfo.ClusterUID)
	assert.Equal(t, "some cluster", clusterInfo.ClusterName)
	assert.Equal(t, "1234", clusterInfo.ClusterVersion)
	assert.Equal(t, "some version", clusterInfo.VizierVersion)

	select {
	case msg := <-subCh:
		req := &messagespb.VizierConnected{}
		err := proto.Unmarshal(msg.Data, req)
		assert.Nil(t, err)
		assert.Equal(t, "1234", req.ResourceVersion)
		assert.Equal(t, "cluster-uid", req.K8sUID)
		assert.Equal(t, "223e4567-e89b-12d3-a456-426655440000", utils.UUIDFromProtoOrNil(req.OrgID).String())
		assert.Equal(t, "123e4567-e89b-12d3-a456-426655440001", utils.UUIDFromProtoOrNil(req.VizierID).String())
		return
	case <-time.After(1 * time.Second):
		t.Fatal("Timed out")
		return
	}
	// TODO(zasgar): write more tests here.
}

func TestServer_HandleVizierHeartbeat(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	s := controller.New(db, "test", mockDNSClient, nc)

	tests := []struct {
		name                      string
		expectGetDNSAddressCalled bool
		dnsAddressResponse        *dnsmgrpb.GetDNSAddressResponse
		dnsAddressError           error
		vizierID                  string
		hbAddress                 string
		hbPort                    int
		updatedClusterStatus      string
		expectedClusterAddress    string
		status                    cvmsgspb.VizierStatus
	}{
		{
			name:                      "valid vizier",
			expectGetDNSAddressCalled: true,
			dnsAddressResponse: &dnsmgrpb.GetDNSAddressResponse{
				DNSAddress: "abc.clusters.dev.withpixie.dev",
			},
			dnsAddressError:        nil,
			vizierID:               "123e4567-e89b-12d3-a456-426655440001",
			hbAddress:              "127.0.0.1",
			hbPort:                 123,
			updatedClusterStatus:   "HEALTHY",
			expectedClusterAddress: "abc.clusters.dev.withpixie.dev:123",
		},
		{
			name:                      "valid vizier dns failed",
			expectGetDNSAddressCalled: true,
			dnsAddressResponse:        nil,
			dnsAddressError:           errors.New("Could not get DNS address"),
			vizierID:                  "123e4567-e89b-12d3-a456-426655440001",
			hbAddress:                 "127.0.0.1",
			hbPort:                    123,
			updatedClusterStatus:      "HEALTHY",
			expectedClusterAddress:    "127.0.0.1:123",
		},
		{
			name:                      "valid vizier no address",
			expectGetDNSAddressCalled: false,
			vizierID:                  "123e4567-e89b-12d3-a456-426655440001",
			hbAddress:                 "",
			hbPort:                    0,
			updatedClusterStatus:      "UNHEALTHY",
			expectedClusterAddress:    "",
		},
		{
			name:                      "unknown vizier",
			expectGetDNSAddressCalled: false,
			vizierID:                  "223e4567-e89b-12d3-a456-426655440001",
			hbAddress:                 "",
			updatedClusterStatus:      "",
			expectedClusterAddress:    "",
			status:                    cvmsgspb.VZ_ST_UPDATING,
		},
		{
			name:                      "updating vizier",
			expectGetDNSAddressCalled: true,
			dnsAddressResponse: &dnsmgrpb.GetDNSAddressResponse{
				DNSAddress: "abc.clusters.dev.withpixie.dev",
			},
			dnsAddressError:        nil,
			vizierID:               "123e4567-e89b-12d3-a456-426655440001",
			hbAddress:              "127.0.0.1",
			hbPort:                 123,
			updatedClusterStatus:   "UPDATING",
			expectedClusterAddress: "abc.clusters.dev.withpixie.dev:123",
			status:                 cvmsgspb.VZ_ST_UPDATING,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			dnsMgrReq := &dnsmgrpb.GetDNSAddressRequest{
				ClusterID: utils.ProtoFromUUIDStrOrNil(tc.vizierID),
				IPAddress: tc.hbAddress,
			}

			if tc.expectGetDNSAddressCalled {
				mockDNSClient.EXPECT().
					GetDNSAddress(gomock.Any(), dnsMgrReq).
					Return(tc.dnsAddressResponse, tc.dnsAddressError)
			}

			nestedMsg := &cvmsgspb.VizierHeartbeat{
				VizierID:       utils.ProtoFromUUIDStrOrNil(tc.vizierID),
				Time:           100,
				SequenceNumber: 200,
				Address:        tc.hbAddress,
				Port:           int32(tc.hbPort),
				Status:         tc.status,
			}
			nestedAny, err := types.MarshalAny(nestedMsg)
			if err != nil {
				t.Fatal("Could not marshal pb")
			}

			req := &cvmsgspb.V2CMessage{
				Msg: nestedAny,
			}

			s.HandleVizierHeartbeat(req)

			// Check database.
			clusterQuery := `SELECT status, address from vizier_cluster_info WHERE vizier_cluster_id=$1`
			var clusterInfo struct {
				Status  string `db:"status"`
				Address string `db:"address"`
			}
			clusterID, err := uuid.FromString(tc.vizierID)
			assert.Nil(t, err)
			err = db.Get(&clusterInfo, clusterQuery, clusterID)
			assert.Equal(t, tc.updatedClusterStatus, clusterInfo.Status)
			assert.Equal(t, tc.expectedClusterAddress, clusterInfo.Address)

		})
	}
}

func TestServer_GetSSLCerts(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	subCh := make(chan *nats.Msg, 1)
	sub, err := nc.ChanSubscribe("c2v.123e4567-e89b-12d3-a456-426655440001.sslResp", subCh)
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}
	defer sub.Unsubscribe()

	s := controller.New(db, "test", mockDNSClient, nc)

	t.Run("dnsmgr error", func(t *testing.T) {
		dnsMgrReq := &dnsmgrpb.GetSSLCertsRequest{
			ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		}

		dnsMgrResp := &dnsmgrpb.GetSSLCertsResponse{
			Key:  "abcd",
			Cert: "efgh",
		}

		mockDNSClient.EXPECT().
			GetSSLCerts(gomock.Any(), dnsMgrReq).
			Return(dnsMgrResp, nil)

		nestedMsg := &cvmsgspb.VizierSSLCertRequest{
			VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
		}
		nestedAny, err := types.MarshalAny(nestedMsg)
		if err != nil {
			t.Fatal("Could not marshal pb")
		}

		req := &cvmsgspb.V2CMessage{
			Msg: nestedAny,
		}

		s.HandleSSLRequest(req)

		// Listen for response.
		select {
		case m := <-subCh:
			pb := &cvmsgspb.C2VMessage{}
			err = proto.Unmarshal(m.Data, pb)
			if err != nil {
				t.Fatal("Could not unmarshal message")
			}
			resp := &cvmsgspb.VizierSSLCertResponse{}
			err = types.UnmarshalAny(pb.Msg, resp)
			if err != nil {
				t.Fatal("Could not unmarshal any message")
			}
			assert.Equal(t, "abcd", resp.Key)
			assert.Equal(t, "efgh", resp.Cert)
		case <-time.After(1 * time.Second):
			t.Fatal("Timeout")
		}
	})
}

func TestServer_UpdateOrInstallVizier(t *testing.T) {
	viper.Set("jwt_signing_key", "jwtkey")

	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	vizierID, _ := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")

	go nc.Subscribe("c2v.123e4567-e89b-12d3-a456-426655440001.VizierUpdate", func(m *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(m.Data, c2vMsg)
		assert.Nil(t, err)
		resp := &cvmsgspb.UpdateOrInstallVizierRequest{}
		err = types.UnmarshalAny(c2vMsg.Msg, resp)
		assert.Equal(t, "0.1.30", resp.Version)
		assert.NotNil(t, resp.Token)
		claims := jwt.MapClaims{}
		_, err = jwt.ParseWithClaims(resp.Token, claims, func(token *jwt.Token) (interface{}, error) {
			return []byte("jwtkey"), nil
		})
		assert.Nil(t, err)
		assert.Equal(t, "user", claims["Scopes"].(string))
		// Send response.
		updateResp := &cvmsgspb.UpdateOrInstallVizierResponse{
			UpdateStarted: true,
		}
		respAnyMsg, err := types.MarshalAny(updateResp)
		assert.Nil(t, err)
		wrappedMsg := &cvmsgspb.V2CMessage{
			VizierID: vizierID.String(),
			Msg:      respAnyMsg,
		}

		b, err := wrappedMsg.Marshal()
		assert.Nil(t, err)
		topic := vzshard.V2CTopic("VizierUpdateResponse", vizierID)
		err = nc.Publish(topic, b)
		assert.Nil(t, err)
	})

	s := controller.New(db, "test", mockDNSClient, nc)

	req := &cvmsgspb.UpdateOrInstallVizierRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil(vizierID.String()),
		Version:  "0.1.30",
	}

	resp, err := s.UpdateOrInstallVizier(CreateTestContext(), req)
	require.Nil(t, err)
	require.NotNil(t, resp)
}

func TestServer_MessageHandler(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDNSClient := mock_dnsmgrpb.NewMockDNSMgrServiceClient(ctrl)

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	subCh := make(chan *nats.Msg, 1)
	sub, err := nc.ChanSubscribe("c2v.123e4567-e89b-12d3-a456-426655440001.sslResp", subCh)
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}
	defer sub.Unsubscribe()

	_ = controller.New(db, "test", mockDNSClient, nc)

	dnsMgrReq := &dnsmgrpb.GetSSLCertsRequest{
		ClusterID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
	}
	dnsMgrResp := &dnsmgrpb.GetSSLCertsResponse{
		Key:  "abcd",
		Cert: "efgh",
	}

	mockDNSClient.EXPECT().
		GetSSLCerts(gomock.Any(), dnsMgrReq).
		Return(dnsMgrResp, nil)

	// Publish v2c message over nats.
	nestedMsg := &cvmsgspb.VizierSSLCertRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
	}
	nestedAny, err := types.MarshalAny(nestedMsg)
	if err != nil {
		t.Fatal("Could not marshal pb")
	}
	wrappedMsg := &cvmsgspb.V2CMessage{
		VizierID: "123e4567-e89b-12d3-a456-426655440001",
		Msg:      nestedAny,
	}

	b, err := wrappedMsg.Marshal()
	if err != nil {
		t.Fatal("Could not marshal message to bytes")
	}
	nc.Publish("v2c.1.123e4567-e89b-12d3-a456-426655440001.ssl", b)

	select {
	case m := <-subCh:
		pb := &cvmsgspb.C2VMessage{}
		err = proto.Unmarshal(m.Data, pb)
		if err != nil {
			t.Fatal("Could not unmarshal message")
		}
		resp := &cvmsgspb.VizierSSLCertResponse{}
		err = types.UnmarshalAny(pb.Msg, resp)
		if err != nil {
			t.Fatal("Could not unmarshal any message")
		}
		assert.Equal(t, "abcd", resp.Key)
		assert.Equal(t, "efgh", resp.Cert)
	case <-time.After(1 * time.Second):
		t.Fatal("Timeout")
	}
}

func TestServer_GetViziersByShard(t *testing.T) {
	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	s := controller.New(db, "test", nil, nil)

	tests := []struct {
		name string

		// Inputs:
		shardID string

		// Outputs:
		expectGRPCError error
		expectResponse  *vzmgrpb.GetViziersByShardResponse
	}{
		{
			name:            "Bad shardID",
			shardID:         "gf",
			expectGRPCError: status.Error(codes.InvalidArgument, "bad arg"),
		},
		{
			name:            "Bad shardID (valid hex, too large)",
			shardID:         "fff",
			expectGRPCError: status.Error(codes.InvalidArgument, "bad arg"),
		},
		{
			name:            "Bad shardID (valid hex, too small)",
			shardID:         "f",
			expectGRPCError: status.Error(codes.InvalidArgument, "bad arg"),
		},
		{
			name:    "Single vizier response",
			shardID: "00",

			expectGRPCError: nil,
			expectResponse: &vzmgrpb.GetViziersByShardResponse{
				Viziers: []*vzmgrpb.GetViziersByShardResponse_VizierInfo{
					{
						VizierID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
						OrgID:    utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					},
				},
			},
		},
		{
			name:    "Multi vizier response",
			shardID: "03",

			expectGRPCError: nil,
			expectResponse: &vzmgrpb.GetViziersByShardResponse{
				Viziers: []*vzmgrpb.GetViziersByShardResponse_VizierInfo{
					{
						VizierID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
						OrgID:    utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					},
					{
						VizierID: utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440003"),
						OrgID:    utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					},
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			req := &vzmgrpb.GetViziersByShardRequest{ShardID: test.shardID}
			resp, err := s.GetViziersByShard(context.Background(), req)

			if test.expectGRPCError != nil {
				assert.Equal(t, status.Code(test.expectGRPCError), status.Code(err))
			} else {
				assert.Equal(t, test.expectResponse, resp)
			}
		})
	}
}
