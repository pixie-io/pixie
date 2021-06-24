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

package apikey

import (
	"context"
	"database/sql"
	"errors"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

var (
	// ErrAPIKeyNotFound is used when the specified API key cannot be located.
	ErrAPIKeyNotFound = errors.New("invalid API key")
)

// Service is used to provision and manage API keys.
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
func (s *Service) Create(ctx context.Context, req *authpb.CreateAPIKeyRequest) (*authpb.APIKey, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}

	var id uuid.UUID
	var ts time.Time
	query := `INSERT INTO api_keys(org_id, user_id, unsalted_key, description) VALUES($1, $2, $3, $4) RETURNING id, created_at`
	keyID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}
	key := keyID.String()
	err = s.db.QueryRowxContext(ctx, query,
		sCtx.Claims.GetUserClaims().OrgID, sCtx.Claims.GetUserClaims().UserID, key, req.Desc).
		Scan(&id, &ts)
	if err != nil {
		log.WithError(err).Error("Failed to insert API keys")
		return nil, status.Error(codes.Internal, "Failed to insert API keys")
	}

	tp, _ := types.TimestampProto(ts)
	return &authpb.APIKey{
		ID:        utils.ProtoFromUUID(id),
		Key:       key,
		CreatedAt: tp,
	}, nil
}

// List returns all the keys belonging to an org.
func (s *Service) List(ctx context.Context, req *authpb.ListAPIKeyRequest) (*authpb.ListAPIKeyResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, err.Error())
	}

	// Return all clusters when the OrgID matches.
	query := `SELECT id, org_id, unsalted_key, created_at, description from api_keys WHERE org_id=$1 ORDER BY created_at`
	rows, err := s.db.QueryxContext(ctx, query, sCtx.Claims.GetUserClaims().OrgID)
	if err != nil {
		if err == sql.ErrNoRows {
			return &authpb.ListAPIKeyResponse{}, nil
		}
		log.WithError(err).Error("Failed to fetch API keys")
		return nil, status.Errorf(codes.Internal, "failed to fetch API keys")
	}
	defer rows.Close()

	keys := make([]*authpb.APIKey, 0)
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
		keys = append(keys, &authpb.APIKey{
			ID:        utils.ProtoFromUUIDStrOrNil(id),
			Key:       key,
			CreatedAt: tProto,
			Desc:      desc,
		})
	}
	return &authpb.ListAPIKeyResponse{
		Keys: keys,
	}, nil
}

// Get returns a specific key if it's owned by the org.
func (s *Service) Get(ctx context.Context, req *authpb.GetAPIKeyRequest) (*authpb.GetAPIKeyResponse, error) {
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
	query := `SELECT unsalted_key, created_at, description from api_keys WHERE org_id=$1 and id=$2`
	err = s.db.QueryRowxContext(ctx, query, sCtx.Claims.GetUserClaims().OrgID, tokenID).Scan(&key, &createdAt, &desc)
	if err != nil {
		return nil, status.Error(codes.NotFound, "No such API key")
	}

	createdAtProto, _ := types.TimestampProto(createdAt)
	return &authpb.GetAPIKeyResponse{Key: &authpb.APIKey{
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

	query := `DELETE from api_keys WHERE org_id=$1 and id=$2`
	res, err := s.db.ExecContext(ctx, query, sCtx.Claims.GetUserClaims().OrgID, tokenID)
	if err != nil {
		log.WithError(err).Error("Failed to delete API token")
		return nil, status.Error(codes.Internal, "failed to delete API token")
	}

	c, err := res.RowsAffected()
	if err != nil {
		log.WithError(err).Error("Failed to delete API token")
		return nil, status.Error(codes.Internal, "failed to delete API token")
	}

	if c == 0 {
		return nil, status.Error(codes.NotFound, "no such token to delete")
	}

	return &types.Empty{}, nil
}

// FetchOrgUserIDUsingAPIKey gets the org and user ID based on the API key.
func (s *Service) FetchOrgUserIDUsingAPIKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, error) {
	query := `SELECT org_id, user_id from api_keys WHERE unsalted_key=$1`
	var orgID uuid.UUID
	var userID uuid.UUID
	err := s.db.QueryRowxContext(ctx, query, key).Scan(&orgID, &userID)
	if err != nil {
		if err == sql.ErrNoRows {
			return uuid.Nil, uuid.Nil, ErrAPIKeyNotFound
		}
		return uuid.Nil, uuid.Nil, err
	}
	return orgID, userID, nil
}
