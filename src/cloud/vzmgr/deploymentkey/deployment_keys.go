package deploymentkey

import (
	"context"
	"database/sql"
	"time"

	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzerrors"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils"
)

// Service is used to provision and manage deployment keys.
type Service struct {
	db    *sqlx.DB
	dbKey string
}

// New creates a new Service.
func New(db *sqlx.DB, dbKey string) *Service {
	return &Service{
		db:    db,
		dbKey: dbKey,
	}
}

// Create a key with the org/user as an owner.
func (s *Service) Create(ctx context.Context, req *vzmgrpb.CreateDeploymentKeyRequest) (*vzmgrpb.DeploymentKey, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}

	var id uuid.UUID
	var ts time.Time
	query := `INSERT INTO vizier_deployment_keys(org_id, user_id, key, description) VALUES($1, $2, PGP_SYM_ENCRYPT($3, $4), $5) RETURNING id, created_at`
	key := uuid.NewV4().String()
	err = s.db.QueryRowxContext(ctx, query,
		sCtx.Claims.GetUserClaims().OrgID, sCtx.Claims.GetUserClaims().UserID, key, s.dbKey, req.Desc).
		Scan(&id, &ts)
	if err != nil {
		log.WithError(err).Error("Failed to insert deployment keys")
		return nil, status.Error(codes.Internal, "Failed to insert deployment keys")
	}

	tp, _ := types.TimestampProto(ts)
	return &vzmgrpb.DeploymentKey{
		ID:        utils.ProtoFromUUID(id),
		Key:       key,
		CreatedAt: tp,
	}, nil
}

// List returns all the keys belonging to an org.
func (s *Service) List(ctx context.Context, req *vzmgrpb.ListDeploymentKeyRequest) (*vzmgrpb.ListDeploymentKeyResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}

	// Return all clusters when the OrgID matches.
	query := `SELECT id, org_id, PGP_SYM_DECRYPT(key::bytea, $1), created_at, description from vizier_deployment_keys WHERE org_id=$2 ORDER BY created_at`
	rows, err := s.db.QueryxContext(ctx, query, s.dbKey, sCtx.Claims.GetUserClaims().OrgID)
	if err != nil {
		if err == sql.ErrNoRows {
			return &vzmgrpb.ListDeploymentKeyResponse{}, nil
		}
		log.WithError(err).Error("Failed to fetch deployment keys")
		return nil, status.Errorf(codes.Internal, "failed to fetch deployment keys")
	}
	defer rows.Close()

	keys := make([]*vzmgrpb.DeploymentKey, 0)
	for rows.Next() {
		var id string
		var orgID string
		var key string
		var createdAt time.Time
		var desc string
		err = rows.Scan(&id, &orgID, &key, &createdAt, &desc)
		if err != nil {
			log.WithError(err).Error("Failed to read data from postgres")
			return nil, status.Error(codes.Internal, "failed to read data")
		}
		tProto, _ := types.TimestampProto(createdAt)
		keys = append(keys, &vzmgrpb.DeploymentKey{
			ID:        utils.ProtoFromUUIDStrOrNil(id),
			Key:       key,
			CreatedAt: tProto,
			Desc:      desc,
		})
	}
	return &vzmgrpb.ListDeploymentKeyResponse{
		Keys: keys,
	}, nil
}

// Get returns a specfic key if it's owned by the org.
func (s *Service) Get(ctx context.Context, req *vzmgrpb.GetDeploymentKeyRequest) (*vzmgrpb.GetDeploymentKeyResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}
	tokenID, err := utils.UUIDFromProto(req.ID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid id format")
	}

	var key string
	var createdAt time.Time
	var desc string
	query := `SELECT PGP_SYM_DECRYPT(key::bytea, $1), created_at, description from vizier_deployment_keys WHERE org_id=$2 and id=$3`
	err = s.db.QueryRowxContext(ctx, query, s.dbKey, sCtx.Claims.GetUserClaims().OrgID, tokenID).Scan(&key, &createdAt, &desc)
	if err != nil {
		return nil, status.Error(codes.NotFound, "No such deployment key")
	}

	createdAtProto, _ := types.TimestampProto(createdAt)
	return &vzmgrpb.GetDeploymentKeyResponse{Key: &vzmgrpb.DeploymentKey{
		ID:        req.ID,
		Key:       key,
		CreatedAt: createdAtProto,
		Desc:      desc,
	}}, nil
}

// Delete will remove the key.
func (s *Service) Delete(ctx context.Context, req *uuidpb.UUID) (*types.Empty, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}

	tokenID, err := utils.UUIDFromProto(req)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid id format")
	}

	query := `DELETE from vizier_deployment_keys WHERE org_id=$1 and id=$2`
	res, err := s.db.ExecContext(ctx, query, sCtx.Claims.GetUserClaims().OrgID, tokenID)
	if err != nil {
		log.WithError(err).Error("Failed to delete deployment token")
		return nil, status.Error(codes.Internal, "failed to delete deployment token")
	}

	c, err := res.RowsAffected()
	if err != nil {
		log.WithError(err).Error("Failed to delete deployment token")
		return nil, status.Error(codes.Internal, "failed to delete deployment token")
	}

	if c == 0 {
		return nil, status.Error(codes.NotFound, "no such token to delete")
	}

	return &types.Empty{}, nil
}

// FetchOrgUserIDUsingDeploymentKey gets the org and user ID based on the deployment key.
func (s *Service) FetchOrgUserIDUsingDeploymentKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, error) {
	query := `SELECT org_id, user_id from vizier_deployment_keys WHERE PGP_SYM_DECRYPT(key::bytea, $2)=$1`
	var orgID uuid.UUID
	var userID uuid.UUID
	err := s.db.QueryRowxContext(ctx, query, key, s.dbKey).Scan(&orgID, &userID)
	if err != nil {
		if err == sql.ErrNoRows {
			return uuid.Nil, uuid.Nil, vzerrors.ErrDeploymentKeyNotFound
		}
		return uuid.Nil, uuid.Nil, err
	}
	return orgID, userID, nil
}
