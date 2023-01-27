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
	"crypto/rand"
	"database/sql"
	"database/sql/driver"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/shared/messages"
	"px.dev/pixie/src/cloud/shared/messagespb"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzmgr/vzerrors"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/events"
	jwtutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/namesgenerator"
)

// SaltLength is the length of the salt used when encrypting the jwt signing key.
const SaltLength int = 10

// DefaultProjectName is the default project name to use for a vizier cluster that is created if none if provided.
const DefaultProjectName = "default"

// HandleNATSMessageFunc is the signature for a NATS message handler.
type HandleNATSMessageFunc func(*cvmsgspb.V2CMessage)

// Server is a bridge implementation of evzmgr.
type Server struct {
	db      *sqlx.DB
	dbKey   string
	nc      *nats.Conn
	updater VzUpdater

	done chan struct{}
	once sync.Once
}

// VzUpdater is the interface for the module responsible for updating Vizier.
type VzUpdater interface {
	UpdateOrInstallVizier(vizierID uuid.UUID, version string, redeployEtcd bool) (*cvmsgspb.V2CMessage, error)
	VersionUpToDate(version string) bool
	// AddToUpdateQueue must be idempotent since we Queue based on heartbeats and reported version.
	AddToUpdateQueue(vizierID uuid.UUID) bool
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string, nc *nats.Conn, updater VzUpdater) *Server {
	s := &Server{
		db:      db,
		dbKey:   dbKey,
		nc:      nc,
		updater: updater,
		done:    make(chan struct{}),
	}

	_ = prometheus.Register(NewStatusMetricsCollector(db))

	for _, shard := range vzshard.GenerateShardRange() {
		s.startShardedHandler(shard, "heartbeat", s.HandleVizierHeartbeat)
	}

	return s
}

// Stop performs any necessary cleanup before shutdown.
func (s *Server) Stop() {
	s.once.Do(func() {
		close(s.done)
	})
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

type vizierStatus cvmsgspb.VizierStatus

func (s vizierStatus) Stringify() string {
	v := cvmsgspb.VizierStatus(s)
	return strings.TrimPrefix(v.String(), "VZ_ST_")
}

func (s vizierStatus) Value() (driver.Value, error) {
	return s.Stringify(), nil
}

func (s *vizierStatus) Scan(value interface{}) error {
	if value == nil {
		*s = vizierStatus(cvmsgspb.VZ_ST_UNKNOWN)
		return nil
	}
	sv, err := driver.String.ConvertValue(value)
	if err != nil {
		return errors.New("failed to scan vizier status")
	}
	v, ok := cvmsgspb.VizierStatus_value[fmt.Sprintf("VZ_ST_%s", sv)]
	if !ok {
		return errors.New("failed to scan vizier status")
	}
	*s = vizierStatus(v)
	return nil
}

func (s vizierStatus) ToProto() cvmsgspb.VizierStatus {
	return cvmsgspb.VizierStatus(s)
}

func validateOrgID(ctx context.Context, providedOrgIDPB *uuidpb.UUID) error {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return err
	}
	claimsOrgIDstr := sCtx.Claims.GetUserClaims().OrgID

	providedOrgID := utils.UUIDFromProtoOrNil(providedOrgIDPB)
	if providedOrgID == uuid.Nil {
		return status.Errorf(codes.InvalidArgument, "invalid org id")
	}
	if providedOrgID.String() != claimsOrgIDstr {
		return status.Errorf(codes.PermissionDenied, "org ids don't match")
	}
	return nil
}

func (s *Server) validateOrgOwnsCluster(ctx context.Context, clusterID *uuidpb.UUID) error {
	clusterUUID := utils.UUIDFromProtoOrNil(clusterID)
	if clusterUUID == uuid.Nil {
		return status.Error(codes.InvalidArgument, "invalid cluster id")
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID

	query := `SELECT EXISTS(SELECT 1 FROM vizier_cluster WHERE id=$1 AND org_id=$2)`

	var exists bool
	err = s.db.QueryRow(query, clusterUUID, orgIDstr).Scan(&exists)
	if err == sql.ErrNoRows || !exists {
		return status.Error(codes.NotFound, "invalid cluster ID for org")
	}
	return nil
}

// CreateVizierCluster creates a new tracked vizier cluster.
func (s *Server) CreateVizierCluster(ctx context.Context, req *vzmgrpb.CreateVizierClusterRequest) (*uuidpb.UUID, error) {
	return nil, status.Errorf(codes.Unimplemented, "Deprecated. Please use `px deploy`")
}

// GetViziersByOrg gets a list of viziers by organization.
func (s *Server) GetViziersByOrg(ctx context.Context, orgID *uuidpb.UUID) (*vzmgrpb.GetViziersByOrgResponse, error) {
	if err := validateOrgID(ctx, orgID); err != nil {
		return nil, err
	}
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
		ids = append(ids, utils.ProtoFromUUID(id))
	}
	return &vzmgrpb.GetViziersByOrgResponse{VizierIDs: ids}, nil
}

// VizierInfo represents all info we want to fetch about a Vizier.
type VizierInfo struct {
	ID                            uuid.UUID     `db:"vizier_cluster_id"`
	Status                        vizierStatus  `db:"status"`
	LastHeartbeat                 *int64        `db:"last_heartbeat"`
	ClusterUID                    *string       `db:"cluster_uid"`
	ClusterName                   *string       `db:"cluster_name"`
	ClusterVersion                *string       `db:"cluster_version"`
	OperatorVersion               *string       `db:"operator_version"`
	VizierVersion                 *string       `db:"vizier_version"`
	StatusMessage                 *string       `db:"status_message"`
	ControlPlanePodStatuses       PodStatuses   `db:"control_plane_pod_statuses"`
	UnhealthyDataPlanePodStatuses PodStatuses   `db:"unhealthy_data_plane_pod_statuses"`
	NumNodes                      int32         `db:"num_nodes"`
	NumInstrumentedNodes          int32         `db:"num_instrumented_nodes"`
	OrgID                         uuid.UUID     `db:"org_id"`
	PrevStatus                    *vizierStatus `db:"prev_status"`
	PrevStatusTime                *time.Time    `db:"prev_status_time"`
}

func vizierInfoToProto(vzInfo VizierInfo) *cvmsgspb.VizierInfo {
	clusterUID := ""
	clusterName := ""
	clusterVersion := ""
	operatorVersion := ""
	vizierVersion := ""
	statusMessage := ""
	var prevStatusTime *types.Timestamp
	var prevStatus cvmsgspb.VizierStatus

	lastHearbeat := int64(-1)
	if vzInfo.LastHeartbeat != nil {
		lastHearbeat = *vzInfo.LastHeartbeat
	}

	if vzInfo.ClusterUID != nil {
		clusterUID = *vzInfo.ClusterUID
	}
	if vzInfo.ClusterName != nil {
		clusterName = *vzInfo.ClusterName
	}
	if vzInfo.ClusterVersion != nil {
		clusterVersion = *vzInfo.ClusterVersion
	}
	if vzInfo.OperatorVersion != nil {
		operatorVersion = *vzInfo.OperatorVersion
	}
	if vzInfo.VizierVersion != nil {
		vizierVersion = *vzInfo.VizierVersion
	}
	if vzInfo.StatusMessage != nil {
		statusMessage = *vzInfo.StatusMessage
	}
	if vzInfo.PrevStatusTime != nil {
		prevStatusTime, _ = types.TimestampProto(*vzInfo.PrevStatusTime)
	}
	if vzInfo.PrevStatus != nil {
		prevStatus = vzInfo.PrevStatus.ToProto()
	}

	return &cvmsgspb.VizierInfo{
		VizierID:                      utils.ProtoFromUUID(vzInfo.ID),
		Status:                        vzInfo.Status.ToProto(),
		LastHeartbeatNs:               lastHearbeat,
		Config:                        &cvmsgspb.VizierConfig{},
		ClusterUID:                    clusterUID,
		ClusterName:                   clusterName,
		ClusterVersion:                clusterVersion,
		OperatorVersion:               operatorVersion,
		VizierVersion:                 vizierVersion,
		StatusMessage:                 statusMessage,
		ControlPlanePodStatuses:       vzInfo.ControlPlanePodStatuses,
		UnhealthyDataPlanePodStatuses: vzInfo.UnhealthyDataPlanePodStatuses,
		NumNodes:                      vzInfo.NumNodes,
		NumInstrumentedNodes:          vzInfo.NumInstrumentedNodes,
		PreviousStatus:                prevStatus,
		PreviousStatusTime:            prevStatusTime,
	}
}

// GetVizierInfos gets the vizier info for multiple viziers.
func (s *Server) GetVizierInfos(ctx context.Context, req *vzmgrpb.GetVizierInfosRequest) (*vzmgrpb.GetVizierInfosResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID

	if len(req.VizierIDs) == 0 {
		return &vzmgrpb.GetVizierInfosResponse{}, nil
	}

	ids := make([]uuid.UUID, len(req.VizierIDs))
	for i, id := range req.VizierIDs {
		ids[i] = utils.UUIDFromProtoOrNil(id)
	}

	strQuery := `SELECT i.vizier_cluster_id, c.cluster_uid, c.cluster_name, i.cluster_version, i.operator_version, i.vizier_version,
			  c.org_id, i.status, (EXTRACT(EPOCH FROM age(now(), i.last_heartbeat))*1E9)::bigint as last_heartbeat,
              i.control_plane_pod_statuses, i.unhealthy_data_plane_pod_statuses,
							i.num_nodes, i.num_instrumented_nodes, i.status_message, i.prev_status, i.prev_status_time
              FROM vizier_cluster_info as i, vizier_cluster as c
              WHERE i.vizier_cluster_id=c.id AND i.vizier_cluster_id IN (?) AND c.org_id='%s'`
	strQuery = fmt.Sprintf(strQuery, orgIDstr)

	query, args, err := sqlx.In(strQuery, ids)
	if err != nil {
		return nil, err
	}
	query = s.db.Rebind(query)
	rows, err := s.db.Queryx(query, args...)
	if err != nil {
		return nil, err
	}

	// Create map of Vizier ID -> VizierInfo, which we can use to return the VizierInfos in the
	// requested order.
	defer rows.Close()
	vzInfoMap := make(map[uuid.UUID]*cvmsgspb.VizierInfo)
	for rows.Next() {
		var vzInfo VizierInfo
		err := rows.StructScan(&vzInfo)
		if err != nil {
			return nil, err
		}

		vzInfoPb := vizierInfoToProto(vzInfo)
		vzInfoMap[vzInfo.ID] = vzInfoPb
	}

	vzInfos := make([]*cvmsgspb.VizierInfo, len(req.VizierIDs))
	for i, id := range ids {
		if val, ok := vzInfoMap[id]; ok {
			vzInfos[i] = val
		} else {
			vzInfos[i] = &cvmsgspb.VizierInfo{}
		}
	}

	return &vzmgrpb.GetVizierInfosResponse{
		VizierInfos: vzInfos,
	}, nil
}

// GetVizierInfo returns info for the specified Vizier.
func (s *Server) GetVizierInfo(ctx context.Context, req *uuidpb.UUID) (*cvmsgspb.VizierInfo, error) {
	if err := s.validateOrgOwnsCluster(ctx, req); err != nil {
		return nil, err
	}

	query := `SELECT i.vizier_cluster_id, c.cluster_uid, c.cluster_name, i.cluster_version, i.operator_version, i.vizier_version,
			  i.status, (EXTRACT(EPOCH FROM age(now(), i.last_heartbeat))*1E9)::bigint as last_heartbeat,
              i.control_plane_pod_statuses, i.unhealthy_data_plane_pod_statuses,
							i.num_nodes, i.num_instrumented_nodes, i.status_message, i.prev_status, i.prev_status_time
              from vizier_cluster_info as i, vizier_cluster as c
              WHERE i.vizier_cluster_id=$1 AND i.vizier_cluster_id=c.id`
	vzInfo := VizierInfo{}
	clusterID, err := utils.UUIDFromProto(req)
	if err != nil {
		return nil, err
	}

	rows, err := s.db.Queryx(query, clusterID)
	if err != nil {
		log.WithError(err).Error("Could not query Vizier info")
		return nil, status.Error(codes.Internal, "could not query for viziers")
	}
	defer rows.Close()

	if rows.Next() {
		err := rows.StructScan(&vzInfo)
		if err != nil {
			log.WithError(err).Error("Could not query Vizier info")
			return nil, status.Error(codes.Internal, "could not query for viziers")
		}

		vzInfoPb := vizierInfoToProto(vzInfo)
		return vzInfoPb, nil
	}
	return nil, status.Error(codes.NotFound, "vizier not found")
}

// UpdateVizierConfig supports updating of the Vizier config.
func (s *Server) UpdateVizierConfig(ctx context.Context, req *cvmsgspb.UpdateVizierConfigRequest) (*cvmsgspb.UpdateVizierConfigResponse, error) {
	if err := s.validateOrgOwnsCluster(ctx, req.VizierID); err != nil {
		return nil, err
	}

	return &cvmsgspb.UpdateVizierConfigResponse{}, nil
}

// GetVizierConnectionInfo gets a viziers connection info,
func (s *Server) GetVizierConnectionInfo(ctx context.Context, req *uuidpb.UUID) (*cvmsgspb.VizierConnectionInfo, error) {
	if err := s.validateOrgOwnsCluster(ctx, req); err != nil {
		return nil, err
	}

	clusterID := utils.UUIDFromProtoOrNil(req)
	if clusterID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "failed to parse cluster id")
	}

	query := `SELECT PGP_SYM_DECRYPT(jwt_signing_key::bytea, $2) as jwt_signing_key from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var info struct {
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
	claims := jwtutils.GenerateJWTForCluster("vizier_cluster", "vizier")
	tokenString, err := jwtutils.SignJWTClaims(claims, jwtKey)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "failed to sign token: %s", err.Error())
	}

	return &cvmsgspb.VizierConnectionInfo{
		Token: tokenString,
	}, nil
}

// GetViziersByShard returns the list of connected Viziers for a given shardID.
func (s *Server) GetViziersByShard(ctx context.Context, req *vzmgrpb.GetViziersByShardRequest) (*vzmgrpb.GetViziersByShardResponse, error) {
	// TODO(zasgar/michelle/philkuz): This end point needs to be protected based on service info. We don't want everyone to be able to access it.
	if len(req.FromShardID) != 2 || len(req.ToShardID) != 2 {
		return nil, status.Error(codes.InvalidArgument, "ShardID must be two hex digits")
	}
	if _, err := hex.DecodeString(req.FromShardID); err != nil {
		return nil, status.Error(codes.InvalidArgument, "ShardID must be two hex digits")
	}
	if _, err := hex.DecodeString(req.ToShardID); err != nil {
		return nil, status.Error(codes.InvalidArgument, "ShardID must be two hex digits")
	}
	toShardID := strings.ToLower(req.ToShardID)
	fromShardID := strings.ToLower(req.FromShardID)

	if fromShardID > toShardID {
		return nil, status.Error(codes.InvalidArgument, "FromShardID must be less than or equal to ToShardID")
	}

	query := `
    SELECT vizier_cluster.id, vizier_cluster.org_id, vizier_cluster.cluster_uid
    FROM vizier_cluster, vizier_cluster_info
    WHERE vizier_cluster_info.vizier_cluster_id=vizier_cluster.id
			AND vizier_cluster_info.status != 'DISCONNECTED'
			AND substring(vizier_cluster.id::text, 35)>=$1
			AND substring(vizier_cluster.id::text, 35)<=$2;`

	rows, err := s.db.Queryx(query, fromShardID, toShardID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	type Result struct {
		VizierID uuid.UUID `db:"id"`
		OrgID    uuid.UUID `db:"org_id"`
		K8sUID   string    `db:"cluster_uid"`
	}
	results := make([]*vzmgrpb.GetViziersByShardResponse_VizierInfo, 0)
	for rows.Next() {
		var result Result
		err = rows.StructScan(&result)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read vizier info")
		}
		results = append(results, &vzmgrpb.GetViziersByShardResponse_VizierInfo{
			VizierID: utils.ProtoFromUUID(result.VizierID),
			OrgID:    utils.ProtoFromUUID(result.OrgID),
			K8sUID:   result.K8sUID,
		})
	}

	return &vzmgrpb.GetViziersByShardResponse{Viziers: results}, nil
}

// VizierConnected is an the request made to the mgr to handle new Vizier connections.
func (s *Server) VizierConnected(ctx context.Context, req *cvmsgspb.RegisterVizierRequest) (*cvmsgspb.RegisterVizierAck, error) {
	vzVersion := ""
	clusterUID := ""
	clusterName := ""

	if req.ClusterInfo != nil {
		vzVersion = req.ClusterInfo.VizierVersion
		clusterUID = req.ClusterInfo.ClusterUID
		clusterName = req.ClusterInfo.ClusterName
	}

	loggerWithCtx := log.WithContext(ctx).
		WithField("VizierID", utils.UUIDFromProtoOrNil(req.VizierID)).
		WithField("ClusterName", clusterName)

	loggerWithCtx.
		Info("Received RegisterVizierRequest")

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
    SET (last_heartbeat, jwt_signing_key, status, vizier_version)  = (
    	NOW(), PGP_SYM_ENCRYPT($2, $3), $4, $5)
    WHERE vizier_cluster_id = $1`

	vzStatus := vizierStatus(cvmsgspb.VZ_ST_CONNECTED)
	res, err := s.db.Exec(query, vizierID, signingKey, s.dbKey, vzStatus, vzVersion)
	if err != nil {
		return nil, err
	}

	count, _ := res.RowsAffected()
	if count == 0 {
		return nil, status.Error(codes.NotFound, "no such cluster")
	}

	// Send a message over NATS to signal that a Vizier has connected.
	query = `SELECT org_id, cluster_name from vizier_cluster WHERE id=$1`
	var val struct {
		OrgID      uuid.UUID `db:"org_id"`
		VizierName string    `db:"cluster_name"`
	}

	rows, err := s.db.Queryx(query, vizierID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return nil, err
		}
	}

	connMsg := messagespb.VizierConnected{
		VizierID: utils.ProtoFromUUID(vizierID),
		OrgID:    utils.ProtoFromUUID(val.OrgID),
		K8sUID:   clusterUID,
	}
	b, err := connMsg.Marshal()
	if err != nil {
		return nil, err
	}
	err = s.nc.Publish(messages.VizierConnectedChannel, b)
	if err != nil {
		return nil, err
	}
	return &cvmsgspb.RegisterVizierAck{
		Status:     cvmsgspb.ST_OK,
		VizierName: val.VizierName,
	}, nil
}

// HandleVizierHeartbeat handles the heartbeat from connected viziers.
func (s *Server) HandleVizierHeartbeat(v2cMsg *cvmsgspb.V2CMessage) {
	anyMsg := v2cMsg.Msg
	req := &cvmsgspb.VizierHeartbeat{}
	err := types.UnmarshalAny(anyMsg, req)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal NATS message")
		return
	}
	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)

	// We want to detect when the record changes, so we need to exhaustively list all the columns except the
	// heartbeat time and status.
	// Note: We don't compare the json fields because they just contain details of the status fields.
	query := `
		UPDATE vizier_cluster_info x
		SET last_heartbeat = $1, status = $2, control_plane_pod_statuses = CASE WHEN $11 THEN $3::json ELSE y.control_plane_pod_statuses END,
			num_nodes = $4, num_instrumented_nodes = $5, auto_update_enabled = $6,
			unhealthy_data_plane_pod_statuses = $7, cluster_version = $8, status_message = $9, operator_version = $12
		FROM (SELECT * FROM vizier_cluster_info WHERE vizier_cluster_id = $10) y
		WHERE x.vizier_cluster_id = y.vizier_cluster_id
		RETURNING (x.status != y.status
		  OR ((x.num_nodes is not NULL) AND (x.num_nodes != y.num_nodes))
		  OR ((x.num_instrumented_nodes is not NULL) AND (x.num_instrumented_nodes != y.num_instrumented_nodes))
		  OR ((x.auto_update_enabled IS NOT NULL) AND (x.auto_update_enabled != y.auto_update_enabled))
		  OR ((x.cluster_version IS NOT NULL) AND (x.cluster_version != y.cluster_version))
		  OR ((x.operator_version IS NOT NULL) AND (x.operator_version != y.operator_version))
		  OR ((x.status_message is not NULL) AND (x.status_message != y.status_message))) as changed, x.vizier_version`

	var info struct {
		Changed bool   `db:"changed"`
		Version string `db:"vizier_version"`
	}

	rows, err := s.db.Queryx(query, time.Now(), vizierStatus(req.Status), PodStatuses(req.PodStatuses), req.NumNodes,
		req.NumInstrumentedNodes, !req.DisableAutoUpdate, PodStatuses(req.UnhealthyDataPlanePodStatuses),
		req.K8sClusterVersion, req.StatusMessage, vizierID, req.PodStatuses != nil, req.OperatorVersion)
	if err != nil {
		log.WithError(err).Error("Could not update vizier heartbeat")
		return
	}
	defer rows.Close()
	if rows.Next() {
		err := rows.StructScan(&info)
		if err != nil {
			log.Error("Failed to scan DB for vizier heartbeat")
			return
		}
	} else {
		log.Error("Vizier not found during heartbeat update")
		return
	}
	// Release the DB connection early.
	rows.Close()

	// Send analytics event for cluster status changes.
	if info.Changed {
		events.Client().Enqueue(&analytics.Track{
			UserId: vizierID.String(),
			Event:  events.VizierStatusChange,
			Properties: analytics.NewProperties().
				Set("cluster_id", vizierID.String()).
				Set("status", vizierStatus(req.Status).Stringify()).
				Set("num_nodes", req.NumNodes).
				Set("num_instrumented_nodes", req.NumInstrumentedNodes).
				Set("k8s_version", req.K8sClusterVersion).
				Set("vizier_version", info.Version).
				Set("operator_version", req.OperatorVersion).
				Set("auto_update_enabled", !req.DisableAutoUpdate).
				Set("status_message", req.StatusMessage),
		})
	}

	if req.Status == cvmsgspb.VZ_ST_UPDATING {
		return
	}

	if !req.DisableAutoUpdate && !s.updater.VersionUpToDate(info.Version) {
		s.updater.AddToUpdateQueue(vizierID)
	}
}

// getServiceCredentials returns JWT credentials for inter-service requests.
func getServiceCredentials(signingKey string) (string, error) {
	claims := jwtutils.GenerateJWTForService("vzmgr Service", viper.GetString("domain_name"))
	return jwtutils.SignJWTClaims(claims, signingKey)
}

// UpdateOrInstallVizier updates or installs the given vizier cluster to the specified version.
func (s *Server) UpdateOrInstallVizier(ctx context.Context, req *cvmsgspb.UpdateOrInstallVizierRequest) (*cvmsgspb.UpdateOrInstallVizierResponse, error) {
	if err := s.validateOrgOwnsCluster(ctx, req.VizierID); err != nil {
		return nil, err
	}

	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)

	v2cMsg, err := s.updater.UpdateOrInstallVizier(vizierID, req.Version, req.RedeployEtcd)
	if err != nil {
		return nil, err
	}

	resp := &cvmsgspb.UpdateOrInstallVizierResponse{}
	err = types.UnmarshalAny(v2cMsg.Msg, resp)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal response message")
		return nil, err
	}

	return resp, nil
}

func findVizierWithUID(ctx context.Context, tx *sqlx.Tx, orgID uuid.UUID, clusterUID string) (uuid.UUID, vizierStatus, error) {
	query := `
       SELECT vizier_cluster.id, status from vizier_cluster, vizier_cluster_info
       WHERE vizier_cluster.id = vizier_cluster_info.vizier_cluster_id
       AND vizier_cluster.org_id = $1
       AND vizier_cluster.cluster_uid = $2
    `

	var vizierID uuid.UUID
	var status vizierStatus
	err := tx.QueryRowxContext(ctx, query, orgID, clusterUID).Scan(&vizierID, &status)
	if err != nil {
		if err == sql.ErrNoRows {
			return uuid.Nil, vizierStatus(cvmsgspb.VZ_ST_UNKNOWN), nil
		}
		return uuid.Nil, vizierStatus(cvmsgspb.VZ_ST_UNKNOWN), vzerrors.ErrInternalDB
	}
	return vizierID, status, nil
}

func findVizierWithEmptyUID(ctx context.Context, tx *sqlx.Tx, orgID uuid.UUID) (uuid.UUID, vizierStatus, error) {
	query := `
       SELECT vizier_cluster.id, status from vizier_cluster, vizier_cluster_info
       WHERE vizier_cluster.id = vizier_cluster_info.vizier_cluster_id
       AND vizier_cluster.org_id = $1
       AND (vizier_cluster.cluster_uid = ''
            OR vizier_cluster.cluster_uid IS NULL)
    `

	var vizierID uuid.UUID
	var status vizierStatus
	rows, err := tx.QueryxContext(ctx, query, orgID)
	if err != nil {
		return uuid.Nil, vizierStatus(cvmsgspb.VZ_ST_UNKNOWN), err
	}
	defer rows.Close()
	for rows.Next() {
		err = rows.Scan(&vizierID, &status)
		if err != nil {
			return uuid.Nil, vizierStatus(cvmsgspb.VZ_ST_UNKNOWN), err
		}
		// Return the first disconnected cluster.
		if status == vizierStatus(cvmsgspb.VZ_ST_DISCONNECTED) {
			return vizierID, status, nil
		}
	}
	// Return a Nil ID.
	return uuid.Nil, vizierStatus(cvmsgspb.VZ_ST_UNKNOWN), nil
}

func setClusterName(ctx context.Context, tx *sqlx.Tx, clusterID uuid.UUID, generateName func(i int) string) (string, error) {
	// Retry a few times until we find a name that doesn't collide.
	finalName := ""
	for rc := 0; rc < 10; rc++ {
		name := generateName(rc)
		var queryName *string
		query := `SELECT cluster_name from vizier_cluster WHERE cluster_name=$1`
		_ = tx.QueryRowxContext(ctx, query, name).Scan(&queryName)

		if queryName == nil { // Name does not exist in the DB.
			finalName = name
			break
		}
	}

	if finalName == "" {
		return "", errors.New("Could not find a unique cluster name")
	}

	query := `UPDATE vizier_cluster SET cluster_name=$1 WHERE id=$2`
	_, err := tx.ExecContext(ctx, query, finalName, clusterID)

	return finalName, err
}

// ProvisionOrClaimVizier provisions a given cluster or returns the ID if it already exists,
func (s *Server) ProvisionOrClaimVizier(ctx context.Context, orgID uuid.UUID, userID uuid.UUID, clusterUID string, clusterName string) (uuid.UUID, string, error) {
	// TODO(zasgar): This duplicates some functionality in the Create function. Will deprecate that Create function soon.
	tx, err := s.db.BeginTxx(ctx, nil)
	if err != nil {
		return uuid.Nil, "", vzerrors.ErrInternalDB
	}
	defer tx.Rollback()

	var clusterID uuid.UUID
	inputName := strings.TrimSpace(clusterName)

	generateFromGivenName := func(i int) string {
		name := inputName
		if i > 0 {
			randName := make([]byte, 4)
			_, err = rand.Read(randName)
			if err != nil {
				log.WithError(err).Error("Error generating random name")
			}
			name = fmt.Sprintf("%s_%x", name, randName)
		}
		return name
	}

	assignNameAndCommit := func() (uuid.UUID, string, error) {
		// Check if cluster already has a name.
		var existingName *string

		query := `SELECT cluster_name from vizier_cluster WHERE id=$1`
		err := tx.QueryRowxContext(ctx, query, clusterID).Scan(&existingName)
		if err != nil {
			return uuid.Nil, "", vzerrors.ErrInternalDB
		}

		if existingName != nil {
			// No input name specified, so no need to change cluster name.
			if inputName == "" {
				return clusterID, *existingName, nil
			}

			// The existing name is already the same as the input name, or a derivation
			// of the input name. This check is not perfect, as it only checks if the input
			// name matches everything before the "_" in the existingName.
			// For example, if the user named their cluster "test_abcd", then tried
			// to rename it to "test", this would count as a match. This is because we
			// cannot distinguish between randomly generated names and actual-unaltered names.
			dbName := *existingName
			if inputName == dbName {
				return clusterID, *existingName, nil
			}
			prefixIndex := strings.LastIndex(dbName, "_")
			if prefixIndex != -1 {
				dbName = dbName[:prefixIndex]
			}
			if inputName == dbName {
				return clusterID, *existingName, nil
			}
		}

		generateNameFunc := namesgenerator.GetRandomName
		if inputName != "" {
			generateNameFunc = generateFromGivenName
		}

		finalName, err := setClusterName(ctx, tx, clusterID, generateNameFunc)
		if err != nil {
			return uuid.Nil, "", vzerrors.ErrInternalDB
		}

		if err := tx.Commit(); err != nil {
			log.WithError(err).Error("Failed to commit transaction")
			return uuid.Nil, "", vzerrors.ErrInternalDB
		}

		events.Client().Enqueue(&analytics.Track{
			UserId: clusterID.String(),
			Event:  events.VizierCreated,
			Properties: analytics.NewProperties().
				Set("cluster_id", clusterID.String()).
				Set("org_id", orgID.String()),
		})
		return clusterID, finalName, nil
	}

	clusterID, status, err := findVizierWithUID(ctx, tx, orgID, clusterUID)
	if err != nil {
		return uuid.Nil, "", err
	}
	if clusterID != uuid.Nil {
		if status != vizierStatus(cvmsgspb.VZ_ST_DISCONNECTED) {
			return uuid.Nil, "", vzerrors.ErrProvisionFailedVizierIsActive
		}
		return assignNameAndCommit()
	}

	clusterID, _, err = findVizierWithEmptyUID(ctx, tx, orgID)
	if err != nil {
		return uuid.Nil, "", err
	}
	if clusterID != uuid.Nil {
		// Set the cluster ID.
		query := `UPDATE vizier_cluster SET cluster_uid=$1 WHERE id=$2`
		rows, err := tx.QueryxContext(ctx, query, clusterUID, clusterID)
		if err != nil {
			return uuid.Nil, "", err
		}
		rows.Close()
		return assignNameAndCommit()
	}

	// Insert new vizier case.
	query := `
    	WITH ins AS (
               INSERT INTO vizier_cluster (org_id, project_name, cluster_uid) VALUES($1, $2, $3) RETURNING id
		)
		INSERT INTO vizier_cluster_info(vizier_cluster_id, status) SELECT id, 'DISCONNECTED' FROM ins RETURNING vizier_cluster_id`
	err = tx.QueryRowContext(ctx, query, orgID, DefaultProjectName, clusterUID).Scan(&clusterID)
	if err != nil {
		return uuid.Nil, "", err
	}
	return assignNameAndCommit()
}

// GetOrgFromVizier fetches the org to which a Vizier belongs. This is intended to be for internal use only.
func (s *Server) GetOrgFromVizier(ctx context.Context, id *uuidpb.UUID) (*vzmgrpb.GetOrgFromVizierResponse, error) {
	query := `SELECT org_id FROM vizier_cluster where id=$1`

	vzID := utils.UUIDFromProtoOrNil(id)
	var orgID uuid.UUID
	err := s.db.QueryRowxContext(ctx, query, vzID).Scan(&orgID)
	if err != nil {
		return nil, err
	}
	return &vzmgrpb.GetOrgFromVizierResponse{OrgID: utils.ProtoFromUUID(orgID)}, nil
}
