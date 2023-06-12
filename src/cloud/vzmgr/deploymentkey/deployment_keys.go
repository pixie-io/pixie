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

package deploymentkey

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/vzmgr/vzerrors"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/utils"
)

const (
	// deployKeyPrefox is applied to all deploy keys to make them easier to identify.
	deployKeyPrefix = "px-dep-"
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
	orgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id format")
	}

	userID, err := utils.UUIDFromProto(req.UserID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid user id format")
	}

	var id uuid.UUID
	var ts time.Time
	query := `INSERT INTO vizier_deployment_keys(org_id, user_id, hashed_key, encrypted_key, description)
                VALUES($1, $2, sha256($3), PGP_SYM_ENCRYPT($3::text, $4::text), $5)
              RETURNING id, created_at`
	keyID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}
	key := deployKeyPrefix + keyID.String()
	err = s.db.QueryRowxContext(ctx, query, orgID, userID, key, s.dbKey, req.Desc).
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
	orgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id format")
	}

	// Return all clusters when the OrgID matches.
	query := `SELECT id, org_id, user_id, created_at, description
                FROM vizier_deployment_keys
                WHERE org_id=$1
                ORDER BY created_at`
	rows, err := s.db.QueryxContext(ctx, query, orgID)
	if err != nil {
		if err == sql.ErrNoRows {
			return &vzmgrpb.ListDeploymentKeyResponse{}, nil
		}
		log.WithError(err).Error("Failed to fetch deployment keys")
		return nil, status.Errorf(codes.Internal, "failed to fetch deployment keys")
	}
	defer rows.Close()

	keys := make([]*vzmgrpb.DeploymentKeyMetadata, 0)
	for rows.Next() {
		var id string
		var orgID uuid.UUID
		var userID uuid.UUID
		var createdAt time.Time
		var desc string
		err = rows.Scan(&id, &orgID, &userID, &createdAt, &desc)
		if err != nil {
			log.WithError(err).Error("Failed to read data from postgres")
			return nil, status.Error(codes.Internal, "failed to read data")
		}
		tProto, _ := types.TimestampProto(createdAt)
		keys = append(keys, &vzmgrpb.DeploymentKeyMetadata{
			ID:        utils.ProtoFromUUIDStrOrNil(id),
			OrgID:     utils.ProtoFromUUID(orgID),
			UserID:    utils.ProtoFromUUID(userID),
			CreatedAt: tProto,
			Desc:      desc,
		})
	}
	return &vzmgrpb.ListDeploymentKeyResponse{
		Keys: keys,
	}, nil
}

// Get returns a specific key if it's owned by the org.
func (s *Service) Get(ctx context.Context, req *vzmgrpb.GetDeploymentKeyRequest) (*vzmgrpb.GetDeploymentKeyResponse, error) {
	orgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id format")
	}
	tokenID, err := utils.UUIDFromProto(req.ID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid id format")
	}

	var userID uuid.UUID
	var key string
	var createdAt time.Time
	var desc string
	query := `SELECT CONVERT_FROM(PGP_SYM_DECRYPT(encrypted_key, $3::text)::bytea, 'UTF8'), user_id, created_at, description
                FROM vizier_deployment_keys
                WHERE org_id=$1 AND id=$2`
	err = s.db.QueryRowxContext(ctx, query, orgID, tokenID, s.dbKey).
		Scan(&key, &userID, &createdAt, &desc)
	if err != nil {
		return nil, status.Error(codes.NotFound, "No such deployment key")
	}

	createdAtProto, _ := types.TimestampProto(createdAt)
	return &vzmgrpb.GetDeploymentKeyResponse{Key: &vzmgrpb.DeploymentKey{
		ID:        req.ID,
		OrgID:     utils.ProtoFromUUID(orgID),
		UserID:    utils.ProtoFromUUID(userID),
		Key:       key,
		CreatedAt: createdAtProto,
		Desc:      desc,
	}}, nil
}

// Delete will remove the key.
func (s *Service) Delete(ctx context.Context, req *vzmgrpb.DeleteDeploymentKeyRequest) (*types.Empty, error) {
	tokenID, err := utils.UUIDFromProto(req.ID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid deploy key id format")
	}
	orgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id format")
	}

	query := `DELETE FROM vizier_deployment_keys
                WHERE org_id=$1 AND id=$2`
	res, err := s.db.ExecContext(ctx, query, orgID, tokenID)
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
func (s *Service) FetchOrgUserIDUsingDeploymentKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, uuid.UUID, error) {
	resp, err := s.fetchDeploymentKeyUsingKeyFromDB(ctx, key)
	if err != nil {
		return uuid.Nil, uuid.Nil, uuid.Nil, err
	}
	oid, err := utils.UUIDFromProto(resp.OrgID)
	if err != nil {
		return uuid.Nil, uuid.Nil, uuid.Nil, err
	}
	uid, err := utils.UUIDFromProto(resp.UserID)
	if err != nil {
		return uuid.Nil, uuid.Nil, uuid.Nil, err
	}
	keyID, err := utils.UUIDFromProto(resp.ID)
	if err != nil {
		return uuid.Nil, uuid.Nil, uuid.Nil, err
	}
	return oid, uid, keyID, nil
}

// LookupDeploymentKey gets the complete Deployment key information using just the Key.
func (s *Service) LookupDeploymentKey(ctx context.Context, req *vzmgrpb.LookupDeploymentKeyRequest) (*vzmgrpb.LookupDeploymentKeyResponse, error) {
	resp, err := s.fetchDeploymentKeyUsingKeyFromDB(ctx, req.Key)
	if err != nil {
		if err == vzerrors.ErrDeploymentKeyNotFound {
			return nil, status.Error(codes.NotFound, "deployment key not found")
		}
		return nil, status.Error(codes.Internal, err.Error())
	}
	return &vzmgrpb.LookupDeploymentKeyResponse{Key: resp}, nil
}

func (s *Service) fetchDeploymentKeyUsingKeyFromDB(ctx context.Context, key string) (*vzmgrpb.DeploymentKey, error) {
	// For backwards compatibility add in deployKeyPrefix the front of the keys.
	if !strings.HasPrefix(key, deployKeyPrefix) {
		key = deployKeyPrefix + key
	}
	var id uuid.UUID
	var orgID uuid.UUID
	var userID uuid.UUID
	var createdAt time.Time
	var desc string
	query := `SELECT id, org_id, user_id, created_at, description
                FROM vizier_deployment_keys
                WHERE hashed_key=sha256($1) AND PGP_SYM_DECRYPT(encrypted_key::bytea, $2::text)::bytea=$1`
	err := s.db.QueryRowxContext(ctx, query, key, s.dbKey).
		Scan(&id, &orgID, &userID, &createdAt, &desc)
	if err != nil {
		if err == sql.ErrNoRows {
			return nil, vzerrors.ErrDeploymentKeyNotFound
		}
		return nil, fmt.Errorf("failed to query database for API key")
	}

	createdAtProto, _ := types.TimestampProto(createdAt)
	return &vzmgrpb.DeploymentKey{
		ID:        utils.ProtoFromUUID(id),
		OrgID:     utils.ProtoFromUUID(orgID),
		UserID:    utils.ProtoFromUUID(userID),
		Key:       key,
		CreatedAt: createdAtProto,
		Desc:      desc,
	}, nil
}
