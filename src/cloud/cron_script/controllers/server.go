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
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/cron_script/cronscriptpb"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/cvmsgs"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/shared/services/authcontext"
	jwtutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

const (
	msgBackoffInitialInterval = 5 * time.Second
	msgBackoffMultiplier      = 2
	msgBackoffMaxElapsedTime  = 3 * time.Minute
	natsWaitTimeout           = 10 * time.Second
)

// HandleNATSMessageFunc is the signature for a NATS message handler.
type HandleNATSMessageFunc func(*cvmsgspb.V2CMessage)

// Server is a bridge implementation of the pluginService.
type Server struct {
	db          *sqlx.DB
	dbKey       string
	nc          *nats.Conn
	vzmgrClient vzmgrpb.VZMgrServiceClient

	done chan struct{}
	once sync.Once
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string, nc *nats.Conn, vzmgrClient vzmgrpb.VZMgrServiceClient) *Server {
	s := &Server{
		db:          db,
		dbKey:       dbKey,
		nc:          nc,
		vzmgrClient: vzmgrClient,
		done:        make(chan struct{}),
	}
	s.handleRequests()

	return s
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

func (s *Server) handleRequests() {
	for _, shard := range vzshard.GenerateShardRange() {
		s.startShardedHandler(shard, cvmsgs.CronScriptChecksumRequestChannel, s.HandleChecksumRequest)
		s.startShardedHandler(shard, cvmsgs.GetCronScriptsRequestChannel, s.HandleScriptsRequest)
	}
}

func (s *Server) startShardedHandler(shard string, topic string, handler HandleNATSMessageFunc) {
	if s.nc == nil {
		return
	}
	natsCh := make(chan *nats.Msg, 8192)
	sub, err := s.nc.ChanSubscribe(fmt.Sprintf("v2c.%s.*.%s", shard, topic), natsCh)
	if err != nil {
		log.WithError(err).Fatal("Failed to subscribe to NATS channel")
	}

	go func() {
		for {
			select {
			case <-s.done:
				sub.Unsubscribe()
				return
			case msg := <-natsCh:
				pb := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, pb)
				if err != nil {
					log.WithError(err).Error("Could not unmarshal message")
				}
				handler(pb)
			}
		}
	}()
}

// HandleChecksumRequest handles incoming requests for cronscript checksums.
func (s *Server) HandleChecksumRequest(msg *cvmsgspb.V2CMessage) {
	anyMsg := msg.Msg
	req := &cvmsgspb.GetCronScriptsChecksumRequest{}
	err := types.UnmarshalAny(anyMsg, req)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal NATS message")
		return
	}

	var scriptMap map[string]*cvmsgspb.CronScript
	scriptMap, err = s.fetchScriptsForVizier(utils.ProtoFromUUIDStrOrNil(msg.VizierID))
	if err != nil {
		log.WithError(err).Error("Failed to fetch scripts for Vizier")
		return
	}

	// Get checksum, send out.
	checksum, err := scripts.ChecksumFromScriptMap(scriptMap)
	if err != nil {
		log.WithError(err).Error("Failed to get checksum")
		return
	}

	resp := &cvmsgspb.GetCronScriptsChecksumResponse{Checksum: checksum}
	c2vAnyMsg, err := types.MarshalAny(resp)
	if err != nil {
		log.WithError(err).Error("Failed to marshal update script response")
		return
	}
	c2vMsg := &cvmsgspb.C2VMessage{
		Msg: c2vAnyMsg,
	}

	vizierUUID := uuid.FromStringOrNil(msg.VizierID)

	b, err := c2vMsg.Marshal()
	if err != nil {
		log.WithError(err).Error("Failed to marshal c2v message")
		return
	}
	err = s.nc.Publish(vzshard.C2VTopic(fmt.Sprintf("%s:%s", cvmsgs.CronScriptChecksumResponseChannel, req.Topic), vizierUUID), b)
	if err != nil {
		log.WithError(err).Error("Failed to publish checksum response")
		return
	}
}

func (s *Server) fetchScriptsForVizier(vizierID *uuidpb.UUID) (map[string]*cvmsgspb.CronScript, error) {
	vizierUUID := utils.UUIDFromProtoOrNil(vizierID)

	// Find org associated with this Vizier.
	claims := jwtutils.GenerateJWTForService("vzmgr Service", viper.GetString("domain_name"))
	token, err := jwtutils.SignJWTClaims(claims, viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}

	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", token))
	resp, err := s.vzmgrClient.GetOrgFromVizier(ctx, vizierID)
	if err != nil {
		log.WithError(err).Error("Could not find Vizier for org")
		return nil, err
	}

	// Fetch all scripts registered to this Vizier.
	query := `SELECT id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, frequency_s FROM cron_scripts WHERE org_id=$2 AND enabled=true`
	rows, err := s.db.Queryx(query, s.dbKey, utils.UUIDFromProtoOrNil(resp.OrgID))
	if err != nil {
		log.WithError(err).Error("Could not fetch scripts for org")
		return nil, err
	}
	defer rows.Close()

	scriptsMap := make(map[string]*cvmsgspb.CronScript)
	for rows.Next() {
		var s CronScript
		err = rows.StructScan(&s)
		if err != nil {
			continue
		}
		// If no cluster IDs are specified, script is registered to all orgs.
		// Otherwise, we should check if this cluster is in the list of clusters.
		if len(s.ClusterIDs) != 0 {
			found := false
			for _, c := range s.ClusterIDs {
				if c == vizierUUID {
					found = true
				}
			}
			if !found {
				continue
			}
		}
		scriptsMap[s.ID.String()] = &cvmsgspb.CronScript{
			ID:         utils.ProtoFromUUID(s.ID),
			Script:     s.Script,
			Configs:    s.ConfigStr,
			FrequencyS: s.FrequencyS,
		}
	}
	return scriptsMap, nil
}

// HandleScriptsRequest handles incoming requests for cron scripts registered to the given vizier.
func (s *Server) HandleScriptsRequest(msg *cvmsgspb.V2CMessage) {
	anyMsg := msg.Msg
	req := &cvmsgspb.GetCronScriptsRequest{}
	err := types.UnmarshalAny(anyMsg, req)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal NATS message")
		return
	}

	var scriptMap map[string]*cvmsgspb.CronScript
	scriptMap, err = s.fetchScriptsForVizier(utils.ProtoFromUUIDStrOrNil(msg.VizierID))
	if err != nil {
		log.WithError(err).Error("Failed to fetch scripts for Vizier")
		return
	}

	resp := &cvmsgspb.GetCronScriptsResponse{Scripts: scriptMap}
	c2vAnyMsg, err := types.MarshalAny(resp)
	if err != nil {
		log.WithError(err).Error("Failed to marshal update script response")
		return
	}
	c2vMsg := &cvmsgspb.C2VMessage{
		Msg: c2vAnyMsg,
	}

	vizierUUID := uuid.FromStringOrNil(msg.VizierID)

	b, err := c2vMsg.Marshal()
	if err != nil {
		log.WithError(err).Error("Failed to marshal c2v message")
		return
	}
	err = s.nc.Publish(vzshard.C2VTopic(fmt.Sprintf("%s:%s", cvmsgs.GetCronScriptsResponseChannel, req.Topic), vizierUUID), b)
	if err != nil {
		log.WithError(err).WithField("vizierID", vizierUUID).WithField("numScripts", len(scriptMap)).Error("Failed to publish script response")
		return
	}
}

// GetScript gets a script stored in the cron script service.
func (s *Server) GetScript(ctx context.Context, req *cronscriptpb.GetScriptRequest) (*cronscriptpb.GetScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if req.OrgID == nil {
		orgID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	}
	scriptID := utils.UUIDFromProtoOrNil(req.ID)

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := s.db.Queryx(query, s.dbKey, orgID, scriptID)
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
			OrgID:      utils.ProtoFromUUID(orgID),
			Script:     script.Script,
			ClusterIDs: clusterIDs,
			Configs:    script.ConfigStr,
			Enabled:    script.Enabled,
			FrequencyS: script.FrequencyS,
		},
	}, nil
}

// GetScripts gets scripts stored in the cron script service, given a set of IDs.
func (s *Server) GetScripts(ctx context.Context, req *cronscriptpb.GetScriptsRequest) (*cronscriptpb.GetScriptsResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}

	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if req.OrgID == nil {
		orgID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	}

	ids := make([]uuid.UUID, len(req.IDs))
	for i, id := range req.IDs {
		ids[i] = utils.UUIDFromProtoOrNil(id)
	}

	strQuery := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, '%s'::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id='%s' AND id IN (?)`
	strQuery = fmt.Sprintf(strQuery, s.dbKey, orgID)

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
			FrequencyS: p.FrequencyS,
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
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if req.OrgID == nil {
		orgID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	}

	clusterIDs := make([]uuid.UUID, len(req.ClusterIDs))
	for i, c := range req.ClusterIDs {
		clusterIDs[i] = utils.UUIDFromProtoOrNil(c)
	}

	query := `INSERT INTO cron_scripts(org_id, script, cluster_ids, configs, enabled, frequency_s) VALUES ($1, $2, $3, PGP_SYM_ENCRYPT($4, $5), $6, $7) RETURNING id`
	rows, err := s.db.Queryx(query, orgID, req.Script, ClusterIDs(clusterIDs), req.Configs, s.dbKey, !req.Disabled, req.FrequencyS)
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
	idPb := utils.ProtoFromUUID(id)

	if !req.Disabled {
		s.sendCronScriptUpdateToViziers(&cvmsgspb.CronScriptUpdate{
			Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
				UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
					Script: &cvmsgspb.CronScript{
						ID:         idPb,
						Script:     req.Script,
						FrequencyS: req.FrequencyS,
						Configs:    req.Configs,
					},
				},
			},
		}, orgID, req.ClusterIDs)
	}

	return &cronscriptpb.CreateScriptResponse{ID: idPb}, nil
}

// UpdateScript updates an existing cron script.
func (s *Server) UpdateScript(ctx context.Context, req *cronscriptpb.UpdateScriptRequest) (*cronscriptpb.UpdateScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if req.OrgID == nil {
		orgID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	}
	scriptID := utils.UUIDFromProtoOrNil(req.ScriptId)

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := s.db.Queryx(query, s.dbKey, orgID, scriptID)
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

	prevClusterIDs := make([]*uuidpb.UUID, len(script.ClusterIDs))
	for i, c := range script.ClusterIDs {
		prevClusterIDs[i] = utils.ProtoFromUUID(c)
	}

	newClusterIDs := make([]*uuidpb.UUID, len(clusterIDs))
	for i, c := range clusterIDs {
		newClusterIDs[i] = utils.ProtoFromUUID(c)
	}

	// Delete from previous viziers.
	s.sendCronScriptUpdateToViziers(&cvmsgspb.CronScriptUpdate{
		Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
			DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
				ScriptID: req.ScriptId,
			},
		},
	}, orgID, prevClusterIDs)

	if enabled {
		s.sendCronScriptUpdateToViziers(&cvmsgspb.CronScriptUpdate{
			Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
				UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
					Script: &cvmsgspb.CronScript{
						ID:         req.ScriptId,
						Script:     contents,
						FrequencyS: freq,
						Configs:    configs,
					},
				},
			},
		}, orgID, newClusterIDs)
	}

	return &cronscriptpb.UpdateScriptResponse{}, nil
}

// DeleteScript deletes a cron script.
func (s *Server) DeleteScript(ctx context.Context, req *cronscriptpb.DeleteScriptRequest) (*cronscriptpb.DeleteScriptResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Unauthenticated")
	}
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if req.OrgID == nil {
		orgID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	}
	scriptID := utils.UUIDFromProtoOrNil(req.ID)

	query := `SELECT cluster_ids FROM cron_scripts WHERE org_id=$1 AND id=$2`
	rows, err := s.db.Queryx(query, orgID, scriptID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to fetch cron script")
	}
	defer rows.Close()
	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "cron script not found")
	}
	var clusterIDs ClusterIDs
	err = rows.Scan(&clusterIDs)
	if err != nil {
		return nil, status.Error(codes.Internal, "Failed to read cron script")
	}
	clusterIDProtos := make([]*uuidpb.UUID, len(clusterIDs))
	for i, c := range clusterIDs {
		clusterIDProtos[i] = utils.ProtoFromUUID(c)
	}

	query = `DELETE FROM cron_scripts WHERE id=$1 AND org_id=$2`
	_, err = s.db.Exec(query, scriptID, orgID)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to delete")
	}

	s.sendCronScriptUpdateToViziers(&cvmsgspb.CronScriptUpdate{
		Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
			DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
				ScriptID: req.ID,
			},
		},
	}, orgID, clusterIDProtos)

	return &cronscriptpb.DeleteScriptResponse{}, nil
}

func (s *Server) sendCronScriptUpdateToViziers(msg *cvmsgspb.CronScriptUpdate, orgID uuid.UUID, clusterIDs []*uuidpb.UUID) {
	msg.RequestID = uuid.Must(uuid.NewV4()).String()
	msg.Timestamp = time.Now().UnixNano()

	c2vAnyMsg, err := types.MarshalAny(msg)
	if err != nil {
		log.WithError(err).Error("Failed to marshal update script msg")
		return
	}
	c2vMsg := &cvmsgspb.C2VMessage{
		Msg: c2vAnyMsg,
	}

	// Get healthy viziers for org.
	svcJWT := jwtutils.GenerateJWTForAPIUser("", orgID.String(), time.Now().Add(time.Minute*10), viper.GetString("domain_name"))
	svcClaims, err := jwtutils.SignJWTClaims(svcJWT, viper.GetString("jwt_signing_key"))
	if err != nil {
		log.WithError(err).Error("Failed to sign claims")
		return
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", svcClaims))

	if len(clusterIDs) == 0 { // If no clusterIDs specified, this message should be sent to all Viziers in the org.
		viziers, err := s.vzmgrClient.GetViziersByOrg(ctx, utils.ProtoFromUUID(orgID))
		if err != nil {
			log.WithError(err).Error("Could not get viziers for org")
			return
		}
		clusterIDs = viziers.VizierIDs
	}

	vzInfoResp, err := s.vzmgrClient.GetVizierInfos(ctx, &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: clusterIDs,
	})
	if err != nil {
		log.WithError(err).Error("Error fetching info for viziers")
		return
	}

	for _, v := range vzInfoResp.VizierInfos {
		vzUUID := utils.UUIDFromProtoOrNil(v.VizierID)
		if v.Status != cvmsgspb.VZ_ST_DISCONNECTED && v.Status != cvmsgspb.VZ_ST_UNKNOWN {
			go s.retryMessageUntilResponse(c2vMsg, vzshard.C2VTopic(cvmsgs.CronScriptUpdatesChannel, vzUUID), vzshard.V2CTopic(fmt.Sprintf("%s:%s", cvmsgs.CronScriptUpdatesResponseChannel, msg.RequestID), vzUUID))
		}
	}
}

func (s *Server) retryMessageUntilResponse(msg *cvmsgspb.C2VMessage, publishTopic string, respTopic string) {
	sendMsg := func() error {
		return s.natsReplyAndResponse(msg, publishTopic, respTopic)
	}

	backOffOpts := backoff.NewExponentialBackOff()
	backOffOpts.InitialInterval = msgBackoffInitialInterval
	backOffOpts.Multiplier = msgBackoffMultiplier
	backOffOpts.MaxElapsedTime = msgBackoffMaxElapsedTime
	err := backoff.Retry(sendMsg, backOffOpts)
	if err != nil {
		log.WithError(err).Info("Failed to get response from Vizier. Assuming disconnected")
	}
}

func (s *Server) natsReplyAndResponse(req *cvmsgspb.C2VMessage, publishTopic string, responseTopic string) error {
	// Subscribe to topic that the response will be sent on.
	subCh := make(chan *nats.Msg, 4096)
	sub, err := s.nc.ChanSubscribe(responseTopic, subCh)
	if err != nil {
		return err
	}
	defer sub.Unsubscribe()

	// Publish request.
	b, err := req.Marshal()
	if err != nil {
		return err
	}

	err = s.nc.Publish(publishTopic, b)
	if err != nil {
		return err
	}

	// Wait for response.
	t := time.NewTimer(natsWaitTimeout)
	defer t.Stop()
	for {
		select {
		case msg := <-subCh:
			v2cMsg := &cvmsgspb.V2CMessage{}
			err := proto.Unmarshal(msg.Data, v2cMsg)
			if err != nil {
				log.WithError(err).Error("Failed to unmarshal v2c message")
				return err
			}
			return nil
		case <-t.C:
			return errors.New("Failed to get response")
		}
	}
}
