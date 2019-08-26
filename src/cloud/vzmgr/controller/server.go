package controller

import (
	"context"
	"database/sql"
	"database/sql/driver"
	"errors"
	"fmt"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

// Server is a controller implementation of vzmgr.
type Server struct {
	db *sqlx.DB
}

// New creates a new server.
func New(db *sqlx.DB) *Server {
	return &Server{db}
}

type vizierStatus cloudpb.VizierInfo_Status

func (s vizierStatus) Value() (driver.Value, error) {
	v := cloudpb.VizierInfo_Status(s)
	switch v {
	case cloudpb.VZ_ST_UNKNOWN:
		return "UNKNOWN", nil
	case cloudpb.VZ_ST_HEALTHY:
		return "HEALTHY", nil
	case cloudpb.VZ_ST_UNHEALTHY:
		return "UNHEALTHY", nil
	case cloudpb.VZ_ST_DISCONNECTED:
		return "DISCONNECTED", nil
	}
	return nil, fmt.Errorf("failed to parse status: %v", s)
}

func (s *vizierStatus) Scan(value interface{}) error {
	if value == nil {
		*s = vizierStatus(cloudpb.VZ_ST_UNKNOWN)
		return nil
	}
	if sv, err := driver.String.ConvertValue(value); err == nil {
		switch sv {
		case "UNKNOWN":
			{
				*s = vizierStatus(cloudpb.VZ_ST_UNKNOWN)
				return nil
			}
		case "HEALTHY":
			{
				*s = vizierStatus(cloudpb.VZ_ST_HEALTHY)
				return nil
			}
		case "UNHEALTHY":
			{
				*s = vizierStatus(cloudpb.VZ_ST_UNHEALTHY)
				return nil
			}
		case "DISCONNECTED":
			{
				*s = vizierStatus(cloudpb.VZ_ST_DISCONNECTED)
				return nil
			}
		}
	}

	return errors.New("failed to scan vizier status")
}

func (s vizierStatus) ToProto() cloudpb.VizierInfo_Status {
	return cloudpb.VizierInfo_Status(s)
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

	query := `INSERT INTO vizier_cluster (org_id) VALUES($1) RETURNING id`
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
func (s *Server) GetVizierInfo(ctx context.Context, req *uuidpb.UUID) (*cloudpb.VizierInfo, error) {
	query := `SELECT vizier_cluster_id, status, last_heartbeat from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var val struct {
		ID            uuid.UUID    `db:"vizier_cluster_id"`
		Status        vizierStatus `db:"status"`
		LastHeartbeat *int64       `db:"last_heartbeat"`
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
			lastHearbeat = *val.LastHeartbeat
		}
		return &cloudpb.VizierInfo{
			VizierID:        utils.ProtoFromUUID(&val.ID),
			Status:          val.Status.ToProto(),
			LastHeartbeatNs: lastHearbeat,
		}, nil
	}
	return nil, status.Error(codes.NotFound, "vizier not found")
}

// GetVizierConnectionInfo gets a viziers connection info,
func (s *Server) GetVizierConnectionInfo(ctx context.Context, req *uuidpb.UUID) (*cloudpb.VizierConnectionInfo, error) {
	clusterID := utils.UUIDFromProtoOrNil(req)
	if clusterID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "failed to parse cluster id")
	}

	query := `SELECT address, jwt_signing_key from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var info struct {
		Address       string `db:"address"`
		JWTSigningKey string `db:"jwt_signing_key"`
	}

	err := s.db.Get(&info, query, clusterID)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, status.Error(codes.NotFound, "no such cluster")
		}
		return nil, err
	}

	// Generate a signed token for this cluster.
	// TODO(zasgar/michelle): Refactor this into utils and make sure clams are correct.
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.StandardClaims{
		Audience:  "pixielabs.ai",
		ExpiresAt: time.Now().Add(time.Hour).Unix(),
		Id:        "vizier_cluster",
		IssuedAt:  time.Now().Unix(),
		Issuer:    "pixielabs.ai",
		Subject:   "pixielabs.ai/vizier",
	})

	tokenString, err := token.SignedString([]byte(info.JWTSigningKey))
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to sign token: %s", err.Error())
	}

	return &cloudpb.VizierConnectionInfo{
		IPAddress: info.Address,
		Token:     tokenString,
	}, nil
}

// VizierConnected is an the request made to the mgr to handle new Vizier connections.
func (s *Server) VizierConnected(ctx context.Context, req *cloudpb.RegisterVizierRequest) (*cloudpb.RegisterVizierAck, error) {
	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)
	query := `
    UPDATE vizier_cluster_info
    SET last_heartbeat = NOW(), address = $2, jwt_signing_key = $3, status = 'HEALTHY'
    WHERE vizier_cluster_id = $1`

	res, err := s.db.Exec(query, vizierID, "address should be here", req.JwtKey)
	if err != nil {
		return nil, err
	}

	count, _ := res.RowsAffected()
	if count == 0 {
		return nil, status.Error(codes.NotFound, "no such cluster")
	}

	return &cloudpb.RegisterVizierAck{Status: cloudpb.ST_OK}, nil
}

// HandleVizierHeartbeat handles the heartbeat from connected viziers.
func (s *Server) HandleVizierHeartbeat(ctx context.Context, req *cloudpb.VizierHeartbeat) (*cloudpb.VizierHeartbeatAck, error) {
	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)
	query := `
    UPDATE vizier_cluster_info
    SET last_heartbeat = NOW(), status = 'HEALTHY'
    WHERE vizier_cluster_id = $1`

	// TODO(zasgar/michelle): handle sequence ID and time.
	res, err := s.db.Exec(query, vizierID)
	if err != nil {
		return &cloudpb.VizierHeartbeatAck{
			Status:         cloudpb.HB_ERROR,
			Time:           time.Now().Unix(),
			SequenceNumber: req.SequenceNumber,
			ErrorMessage:   "internal error, failed to update heartbeat",
		}, nil
	}

	count, _ := res.RowsAffected()
	if count == 0 {
		return nil, status.Error(codes.NotFound, "vizier not found")
	}

	return &cloudpb.VizierHeartbeatAck{
		Status:         cloudpb.HB_OK,
		Time:           time.Now().Unix(),
		SequenceNumber: req.SequenceNumber,
	}, nil
}
