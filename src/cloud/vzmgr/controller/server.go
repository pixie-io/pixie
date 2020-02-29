package controller

import (
	"context"
	"crypto/rand"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"time"

	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	dnsmgr "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	jwtutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils"
)

// SaltLength is the length of the salt used when encrypting the jwt signing key.
const SaltLength int = 10

// Server is a controller implementation of vzmgr.
type Server struct {
	db           *sqlx.DB
	dbKey        string
	dnsMgrClient dnsmgr.DNSMgrServiceClient
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string, dnsMgrClient dnsmgr.DNSMgrServiceClient) *Server {
	return &Server{db, dbKey, dnsMgrClient}
}

type vizierStatus cvmsgspb.VizierInfo_Status

func (s vizierStatus) Value() (driver.Value, error) {
	v := cvmsgspb.VizierInfo_Status(s)
	switch v {
	case cvmsgspb.VZ_ST_UNKNOWN:
		return "UNKNOWN", nil
	case cvmsgspb.VZ_ST_HEALTHY:
		return "HEALTHY", nil
	case cvmsgspb.VZ_ST_UNHEALTHY:
		return "UNHEALTHY", nil
	case cvmsgspb.VZ_ST_DISCONNECTED:
		return "DISCONNECTED", nil
	}
	return nil, fmt.Errorf("failed to parse status: %v", s)
}

func (s *vizierStatus) Scan(value interface{}) error {
	if value == nil {
		*s = vizierStatus(cvmsgspb.VZ_ST_UNKNOWN)
		return nil
	}
	if sv, err := driver.String.ConvertValue(value); err == nil {
		switch sv {
		case "UNKNOWN":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_UNKNOWN)
				return nil
			}
		case "HEALTHY":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_HEALTHY)
				return nil
			}
		case "UNHEALTHY":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_UNHEALTHY)
				return nil
			}
		case "DISCONNECTED":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_DISCONNECTED)
				return nil
			}
		}
	}

	return errors.New("failed to scan vizier status")
}

func (s vizierStatus) ToProto() cvmsgspb.VizierInfo_Status {
	return cvmsgspb.VizierInfo_Status(s)
}

// CreateVizierCluster creates a new tracked vizier cluster.
func (s *Server) CreateVizierCluster(ctx context.Context, req *vzmgrpb.CreateVizierClusterRequest) (*uuidpb.UUID, error) {
	// TODO(zasgar): AUTH, reject requests without permission for current ORG.
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if orgID == uuid.Nil {
		return nil, status.Errorf(codes.InvalidArgument, "invalid org id")
	}

	tx, err := s.db.BeginTxx(ctx, nil)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to create transaction")
	}
	defer tx.Rollback()

	query := `
    	WITH ins AS (
      		INSERT INTO vizier_cluster (org_id) VALUES($1) RETURNING id
		)
		INSERT INTO vizier_cluster_info(vizier_cluster_id, status) SELECT id, 'DISCONNECTED'  FROM ins RETURNING vizier_cluster_id`
	row, err := s.db.Queryx(query, orgID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, err.Error())
	}
	defer row.Close()

	if row.Next() {
		var id uuid.UUID
		if err := row.Scan(&id); err != nil {
			return nil, err
		}
		tx.Commit()
		return utils.ProtoFromUUID(&id), nil
	}
	return nil, status.Error(codes.Internal, "failed to read cluster id")
}

// GetViziersByOrg gets a list of viziers by organization.
func (s *Server) GetViziersByOrg(ctx context.Context, orgID *uuidpb.UUID) (*vzmgrpb.GetViziersByOrgResponse, error) {
	query := `SELECT id from vizier_cluster WHERE org_id=$1`
	parsedID := utils.UUIDFromProtoOrNil(orgID)
	if parsedID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id")
	}
	rows, err := s.db.Queryx(query, utils.UUIDFromProtoOrNil(orgID))
	if err != nil {
		if err == sql.ErrNoRows {
			return &vzmgrpb.GetViziersByOrgResponse{VizierIDs: nil}, nil
		}
		return nil, status.Errorf(codes.Internal, "failed to fetch viziers by org: %s", err.Error())
	}
	defer rows.Close()

	ids := []*uuidpb.UUID{}
	for rows.Next() {
		var id uuid.UUID
		err = rows.Scan(&id)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read ids")
		}
		ids = append(ids, utils.ProtoFromUUID(&id))
	}
	return &vzmgrpb.GetViziersByOrgResponse{VizierIDs: ids}, nil
}

// GetVizierInfo returns info for the specified Vizier.
func (s *Server) GetVizierInfo(ctx context.Context, req *uuidpb.UUID) (*cvmsgspb.VizierInfo, error) {
	query := `SELECT vizier_cluster_id, status, last_heartbeat from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var val struct {
		ID            uuid.UUID    `db:"vizier_cluster_id"`
		Status        vizierStatus `db:"status"`
		LastHeartbeat *time.Time   `db:"last_heartbeat"`
	}
	clusterID, err := utils.UUIDFromProto(req)
	if err != nil {
		return nil, err
	}

	rows, err := s.db.Queryx(query, clusterID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return nil, err
		}
		lastHearbeat := int64(0)
		if val.LastHeartbeat != nil {
			lastHearbeat = val.LastHeartbeat.UnixNano()
		}
		return &cvmsgspb.VizierInfo{
			VizierID:        utils.ProtoFromUUID(&val.ID),
			Status:          val.Status.ToProto(),
			LastHeartbeatNs: lastHearbeat,
		}, nil
	}
	return nil, status.Error(codes.NotFound, "vizier not found")
}

// GetVizierConnectionInfo gets a viziers connection info,
func (s *Server) GetVizierConnectionInfo(ctx context.Context, req *uuidpb.UUID) (*cvmsgspb.VizierConnectionInfo, error) {
	clusterID := utils.UUIDFromProtoOrNil(req)
	if clusterID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "failed to parse cluster id")
	}

	query := `SELECT address, PGP_SYM_DECRYPT(jwt_signing_key::bytea, $2) as jwt_signing_key from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var info struct {
		Address       string `db:"address"`
		JWTSigningKey string `db:"jwt_signing_key"`
	}

	err := s.db.Get(&info, query, clusterID, s.dbKey)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, status.Error(codes.NotFound, "no such cluster")
		}
		return nil, err
	}

	// Generate a signed token for this cluster.
	jwtKey := info.JWTSigningKey[SaltLength:]
	claims := jwtutils.GenerateJWTForCluster("vizier_cluster")
	tokenString, err := jwtutils.SignJWTClaims(claims, jwtKey)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to sign token: %s", err.Error())
	}

	addr := info.Address
	if addr != "" {
		addr = "https://" + addr
	}

	return &cvmsgspb.VizierConnectionInfo{
		IPAddress: addr,
		Token:     tokenString,
	}, nil
}

// VizierConnected is an the request made to the mgr to handle new Vizier connections.
func (s *Server) VizierConnected(ctx context.Context, req *cvmsgspb.RegisterVizierRequest) (*cvmsgspb.RegisterVizierAck, error) {
	// Add a salt to the signing key.
	salt := make([]byte, SaltLength/2)
	_, err := rand.Read(salt)
	if err != nil {
		return nil, status.Error(codes.Internal, "could not create salt")
	}
	signingKey := fmt.Sprintf("%x", salt) + req.JwtKey

	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)
	query := `
    UPDATE vizier_cluster_info
    SET (last_heartbeat, address, jwt_signing_key, status)  = (
    	NOW(), $2, PGP_SYM_ENCRYPT($3, $4), $5)
    WHERE vizier_cluster_id = $1`

	vzStatus := "HEALTHY"
	if req.Address == "" {
		vzStatus = "UNHEALTHY"
	}

	res, err := s.db.Exec(query, vizierID, req.Address, signingKey, s.dbKey, vzStatus)
	if err != nil {
		return nil, err
	}

	count, _ := res.RowsAffected()
	if count == 0 {
		return nil, status.Error(codes.NotFound, "no such cluster")
	}

	return &cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_OK}, nil
}

// HandleVizierHeartbeat handles the heartbeat from connected viziers.
func (s *Server) HandleVizierHeartbeat(ctx context.Context, req *cvmsgspb.VizierHeartbeat) (*cvmsgspb.VizierHeartbeatAck, error) {
	// Send DNS address.
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	addr := req.Address
	if req.Address != "" {
		dnsMgrReq := &dnsmgr.GetDNSAddressRequest{
			ClusterID: req.VizierID,
			IPAddress: req.Address,
		}
		resp, err := s.dnsMgrClient.GetDNSAddress(ctx, dnsMgrReq)
		if err == nil {
			addr = resp.DNSAddress
		}
	}
	if req.Port != int32(0) {
		addr = fmt.Sprintf("%s:%d", addr, req.Port)
	}

	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)
	query := `
    UPDATE vizier_cluster_info
    SET last_heartbeat = NOW(), status = $1, address= $2
    WHERE vizier_cluster_id = $3`

	vzStatus := "HEALTHY"
	if req.Address == "" {
		vzStatus = "UNHEALTHY"
	}

	// TODO(zasgar/michelle): handle sequence ID and time.
	res, err := s.db.Exec(query, vzStatus, addr, vizierID)
	if err != nil {
		return &cvmsgspb.VizierHeartbeatAck{
			Status:         cvmsgspb.HB_ERROR,
			Time:           time.Now().Unix(),
			SequenceNumber: req.SequenceNumber,
			ErrorMessage:   "internal error, failed to update heartbeat",
		}, nil
	}

	count, _ := res.RowsAffected()
	if count == 0 {
		return nil, status.Error(codes.NotFound, "vizier not found")
	}

	return &cvmsgspb.VizierHeartbeatAck{
		Status:         cvmsgspb.HB_OK,
		Time:           time.Now().Unix(),
		SequenceNumber: req.SequenceNumber,
	}, nil
}

// GetSSLCerts registers certs for the vizier cluster.
func (s *Server) GetSSLCerts(ctx context.Context, req *vzmgrpb.GetSSLCertsRequest) (*vzmgrpb.GetSSLCertsResponse, error) {
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	dnsMgrReq := &dnsmgr.GetSSLCertsRequest{ClusterID: req.ClusterID}
	resp, err := s.dnsMgrClient.GetSSLCerts(ctx, dnsMgrReq)
	if err != nil {
		return nil, err
	}

	return &vzmgrpb.GetSSLCertsResponse{
		Key:  resp.Key,
		Cert: resp.Cert,
	}, nil
}

// getServiceCredentials returns JWT credentials for inter-service requests.
func getServiceCredentials(signingKey string) (string, error) {
	claims := jwtutils.GenerateJWTForService("vzmgr Service")
	return jwtutils.SignJWTClaims(claims, signingKey)
}
