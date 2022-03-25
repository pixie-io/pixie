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

package controllers

import (
	"context"
	"fmt"
	"sync"

	"github.com/gofrs/uuid"
	"github.com/jmoiron/sqlx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/cron_script/cronscriptpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// Server is a bridge implementation of the pluginService.
type Server struct {
	db    *sqlx.DB
	dbKey string

	done chan struct{}
	once sync.Once
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string) *Server {
	return &Server{
		db:    db,
		dbKey: dbKey,
		done:  make(chan struct{}),
	}
}

// Stop performs any necessary cleanup before shutdown.
func (s *Server) Stop() {
	s.once.Do(func() {
		close(s.done)
	})
}

// CronScript contains metadata about a regularly scheduled script.
type CronScript struct {
	ID         uuid.UUID  `db:"id"`
	OrgID      uuid.UUID  `db:"org_id"`
	Script     string     `db:"script"`
	ClusterIDs ClusterIDs `db:"cluster_ids"`
	ConfigStr  string     `db:"configs"`
	Enabled    bool       `db:"enabled"`
	FrequencyS int64      `db:"frequency_s"`
}

// GetScript gets a script stored in the cron script service.
func (s *Server) GetScript(ctx context.Context, req *cronscriptpb.GetScriptRequest) (*cronscriptpb.GetScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	scriptID := utils.UUIDFromProtoOrNil(req.ID)

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := s.db.Queryx(query, s.dbKey, claimsOrgID, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch cron script")
	}
	defer rows.Close()
	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "cron script not found")
	}

	var script CronScript
	err = rows.StructScan(&script)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to read cron script")
	}

	clusterIDs := make([]*uuidpb.UUID, len(script.ClusterIDs))
	for i, c := range script.ClusterIDs {
		clusterIDs[i] = utils.ProtoFromUUID(c)
	}

	return &cronscriptpb.GetScriptResponse{
		Script: &cronscriptpb.CronScript{
			ID:         req.ID,
			OrgID:      utils.ProtoFromUUID(claimsOrgID),
			Script:     script.Script,
			ClusterIDs: clusterIDs,
			Configs:    script.ConfigStr,
			Enabled:    script.Enabled,
		},
	}, nil
}

// GetScripts gets scripts stored in the cron script service, given a set of IDs.
func (s *Server) GetScripts(ctx context.Context, req *cronscriptpb.GetScriptsRequest) (*cronscriptpb.GetScriptsResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}

	ids := make([]uuid.UUID, len(req.IDs))
	for i, id := range req.IDs {
		ids[i] = utils.UUIDFromProtoOrNil(id)
	}

	strQuery := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, '%s'::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id='%s' AND id IN (?)`
	strQuery = fmt.Sprintf(strQuery, s.dbKey, sCtx.Claims.GetUserClaims().OrgID)

	query, args, err := sqlx.In(strQuery, ids)
	if err != nil {
		return nil, status.Error(codes.Internal, "Failed to get cron scripts")
	}
	query = s.db.Rebind(query)
	rows, err := s.db.Queryx(query, args...)
	if err != nil {
		return nil, status.Error(codes.Internal, "Failed to get cron scripts")
	}

	defer rows.Close()

	scripts := []*cronscriptpb.CronScript{}
	for rows.Next() {
		var p CronScript
		err = rows.StructScan(&p)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read scripts")
		}

		clusterIDs := make([]*uuidpb.UUID, len(p.ClusterIDs))
		for i, c := range p.ClusterIDs {
			clusterIDs[i] = utils.ProtoFromUUID(c)
		}

		cpb := &cronscriptpb.CronScript{
			ID:         utils.ProtoFromUUID(p.ID),
			OrgID:      utils.ProtoFromUUID(p.OrgID),
			Script:     p.Script,
			ClusterIDs: clusterIDs,
			Configs:    p.ConfigStr,
			Enabled:    p.Enabled,
		}
		scripts = append(scripts, cpb)
	}
	return &cronscriptpb.GetScriptsResponse{
		Scripts: scripts,
	}, nil
}

// CreateScript creates a cron script.
func (s *Server) CreateScript(ctx context.Context, req *cronscriptpb.CreateScriptRequest) (*cronscriptpb.CreateScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)

	clusterIDs := make([]uuid.UUID, len(req.ClusterIDs))
	for i, c := range req.ClusterIDs {
		clusterIDs[i] = utils.UUIDFromProtoOrNil(c)
	}

	query := `INSERT INTO cron_scripts(org_id, script, cluster_ids, configs, enabled, frequency_s) VALUES ($1, $2, $3, PGP_SYM_ENCRYPT($4, $5), $6, $7) RETURNING id`
	rows, err := s.db.Queryx(query, claimsOrgID, req.Script, ClusterIDs(clusterIDs), req.Configs, s.dbKey, true, req.FrequencyS)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to create cron script")
	}

	defer rows.Close()
	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "Failed to create cron script")
	}

	var id uuid.UUID
	err = rows.Scan(&id)
	if err != nil {
		return nil, status.Error(codes.NotFound, "Failed to create cron script")
	}

	return &cronscriptpb.CreateScriptResponse{ID: utils.ProtoFromUUID(id)}, nil
}

// UpdateScript updates an existing cron script.
func (s *Server) UpdateScript(ctx context.Context, req *cronscriptpb.UpdateScriptRequest) (*cronscriptpb.UpdateScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	scriptID := utils.UUIDFromProtoOrNil(req.ScriptId)

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := s.db.Queryx(query, s.dbKey, claimsOrgID, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch cron script")
	}
	defer rows.Close()
	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "cron script not found")
	}

	var script CronScript
	err = rows.StructScan(&script)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to read cron script")
	}

	contents := script.Script
	if req.Script != nil {
		contents = req.Script.Value
	}

	configs := script.ConfigStr
	if req.Configs != nil {
		configs = req.Configs.Value
	}

	enabled := script.Enabled
	if req.Enabled != nil {
		enabled = req.Enabled.Value
	}

	freq := script.FrequencyS
	if req.FrequencyS != nil {
		freq = req.FrequencyS.Value
	}

	clusterIDs := script.ClusterIDs
	if req.ClusterIDs != nil {
		clusterIDs = make([]uuid.UUID, len(req.ClusterIDs.Value))
		for i, c := range req.ClusterIDs.Value {
			clusterIDs[i] = utils.UUIDFromProtoOrNil(c)
		}
	}

	query = `UPDATE cron_scripts SET script = $1, configs = PGP_SYM_ENCRYPT($2, $3), enabled = $4, frequency_s = $5, cluster_ids=$6 WHERE id = $7`
	_, err = s.db.Exec(query, contents, configs, s.dbKey, enabled, freq, ClusterIDs(clusterIDs), scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to update cron script")
	}

	return &cronscriptpb.UpdateScriptResponse{}, nil
}

// DeleteScript deletes a cron script.
func (s *Server) DeleteScript(ctx context.Context, req *cronscriptpb.DeleteScriptRequest) (*cronscriptpb.DeleteScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	scriptID := utils.UUIDFromProtoOrNil(req.ID)

	query := `DELETE FROM cron_scripts WHERE id=$1 AND org_id=$2`
	_, err = s.db.Exec(query, scriptID, claimsOrgID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to delete")
	}

	return &cronscriptpb.DeleteScriptResponse{}, nil
}
