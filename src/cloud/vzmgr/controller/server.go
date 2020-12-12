package controller

import (
	"context"
	"crypto/rand"
	"database/sql"
	"database/sql/driver"
	"encoding/hex"
	"errors"
	"fmt"
	"strings"

	"github.com/docker/docker/pkg/namesgenerator"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"gopkg.in/segmentio/analytics-go.v3"

	dnsmgr "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/shared/messages"
	messagespb "pixielabs.ai/pixielabs/src/cloud/shared/messagespb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzerrors"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/events"
	jwtutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils"
)

// SaltLength is the length of the salt used when encrypting the jwt signing key.
const SaltLength int = 10

// DefaultProjectName is the default project name to use for a vizier cluster that is created if none if provided.
const DefaultProjectName = "default"

// HandleNATSMessageFunc is the signature for a NATS message handler.
type HandleNATSMessageFunc func(*cvmsgspb.V2CMessage)

// Server is a bridge implementation of evzmgr.
type Server struct {
	db            *sqlx.DB
	dbKey         string
	dnsMgrClient  dnsmgr.DNSMgrServiceClient
	nc            *nats.Conn
	natsCh        chan *nats.Msg
	natsSubs      []*nats.Subscription
	msgHandlerMap map[string]HandleNATSMessageFunc
	updater       VzUpdater
}

// VzUpdater is the interface for the module responsible for updating Vizier.
type VzUpdater interface {
	UpdateOrInstallVizier(vizierID uuid.UUID, version string, redeployEtcd bool) (*cvmsgspb.V2CMessage, error)
	VersionUpToDate(version string) bool
	AddToUpdateQueue(vizierID uuid.UUID) bool
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string, dnsMgrClient dnsmgr.DNSMgrServiceClient, nc *nats.Conn, updater VzUpdater) *Server {
	natsSubs := make([]*nats.Subscription, 0)
	natsCh := make(chan *nats.Msg, 1024)
	msgHandlerMap := make(map[string]HandleNATSMessageFunc)
	s := &Server{db, dbKey, dnsMgrClient, nc, natsCh, natsSubs, msgHandlerMap, updater}

	// Register NATS message handlers.
	if nc != nil {
		s.registerMessageHandler("heartbeat", s.HandleVizierHeartbeat)
		s.registerMessageHandler("ssl", s.HandleSSLRequest)

		go s.handleMessageBus()
	}

	return s
}

func (s *Server) registerMessageHandler(topic string, fn HandleNATSMessageFunc) {
	sub, err := s.nc.ChanSubscribe(fmt.Sprintf("v2c.*.*.%s", topic), s.natsCh)
	if err != nil {
		log.WithError(err).Fatal("Failed to subscribe to NATS channel")
	}
	s.natsSubs = append(s.natsSubs, sub)
	s.msgHandlerMap[topic] = fn
}

func (s *Server) handleMessageBus() {
	defer func() {
		for _, sub := range s.natsSubs {
			sub.Unsubscribe()
		}
	}()
	for {
		select {
		case msg := <-s.natsCh:
			log.WithField("message", msg).Info("Got NATS message")
			// Get topic.
			splitTopic := strings.Split(msg.Subject, ".")
			topic := splitTopic[len(splitTopic)-1]

			pb := &cvmsgspb.V2CMessage{}
			err := proto.Unmarshal(msg.Data, pb)
			if err != nil {
				log.WithError(err).Error("Could not unmarshal message")
			}

			if handler, ok := s.msgHandlerMap[topic]; ok {
				handler(pb)
			} else {
				log.WithField("topic", msg.Subject).Error("Could not find handler for topic")
			}
		}
	}
}

func (s *Server) sendNATSMessage(topic string, msg *types.Any, vizierID uuid.UUID) {
	wrappedMsg := &cvmsgspb.C2VMessage{
		VizierID: vizierID.String(),
		Msg:      msg,
	}

	b, err := wrappedMsg.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshal message to bytes")
		return
	}
	topic = vzshard.C2VTopic(topic, vizierID)
	log.WithField("topic", topic).Info("Sending message")
	err = s.nc.Publish(topic, b)

	if err != nil {
		log.WithError(err).Error("Could not publish message to nats")
	}
}

type vizierStatus cvmsgspb.VizierStatus

func (s vizierStatus) Value() (driver.Value, error) {
	v := cvmsgspb.VizierStatus(s)
	switch v {
	case cvmsgspb.VZ_ST_UNKNOWN:
		return "UNKNOWN", nil
	case cvmsgspb.VZ_ST_HEALTHY:
		return "HEALTHY", nil
	case cvmsgspb.VZ_ST_UNHEALTHY:
		return "UNHEALTHY", nil
	case cvmsgspb.VZ_ST_DISCONNECTED:
		return "DISCONNECTED", nil
	case cvmsgspb.VZ_ST_UPDATING:
		return "UPDATING", nil
	case cvmsgspb.VZ_ST_CONNECTED:
		return "CONNECTED", nil
	case cvmsgspb.VZ_ST_UPDATE_FAILED:
		return "UPDATE_FAILED", nil
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
		case "UPDATING":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_UPDATING)
				return nil
			}
		case "CONNECTED":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_CONNECTED)
				return nil
			}
		case "UPDATE_FAILED":
			{
				*s = vizierStatus(cvmsgspb.VZ_ST_UPDATE_FAILED)
				return nil
			}
		}
	}

	return errors.New("failed to scan vizier status")
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
	sCtx, err := authcontext.FromContext(ctx)
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID

	query := `SELECT org_id from vizier_cluster WHERE id=$1`
	parsedID := utils.UUIDFromProtoOrNil(clusterID)

	if parsedID == uuid.Nil {
		return status.Error(codes.InvalidArgument, "invalid cluster id")
	}

	// Say not found for clusters that this user doesn't have permission for.
	retError := status.Error(codes.NotFound, "invalid cluster ID for org")

	var actualIDStr string
	err = s.db.QueryRowx(query, parsedID).Scan(&actualIDStr)
	if err != nil {
		if err == sql.ErrNoRows {
			return retError
		}
		return status.Errorf(codes.Internal, "failed to fetch viziers by ID: %s", err.Error())
	}
	if orgIDstr != actualIDStr {
		return retError
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
		ids = append(ids, utils.ProtoFromUUID(&id))
	}
	return &vzmgrpb.GetViziersByOrgResponse{VizierIDs: ids}, nil
}

// GetVizierInfo returns info for the specified Vizier.
func (s *Server) GetVizierInfo(ctx context.Context, req *uuidpb.UUID) (*cvmsgspb.VizierInfo, error) {
	if err := s.validateOrgOwnsCluster(ctx, req); err != nil {
		return nil, err
	}

	query := `SELECT i.vizier_cluster_id, c.cluster_uid, c.cluster_name, c.cluster_version, i.vizier_version,
			  i.status, (EXTRACT(EPOCH FROM age(now(), i.last_heartbeat))*1E9)::bigint as last_heartbeat,
              i.passthrough_enabled, i.control_plane_pod_statuses, num_nodes, num_instrumented_nodes
              from vizier_cluster_info as i, vizier_cluster as c
              WHERE i.vizier_cluster_id=$1 AND i.vizier_cluster_id=c.id`
	var val struct {
		ID                      uuid.UUID    `db:"vizier_cluster_id"`
		Status                  vizierStatus `db:"status"`
		LastHeartbeat           *int64       `db:"last_heartbeat"`
		PassthroughEnabled      bool         `db:"passthrough_enabled"`
		ClusterUID              *string      `db:"cluster_uid"`
		ClusterName             *string      `db:"cluster_name"`
		ClusterVersion          *string      `db:"cluster_version"`
		VizierVersion           *string      `db:"vizier_version"`
		ControlPlanePodStatuses PodStatuses  `db:"control_plane_pod_statuses"`
		NumNodes                int32        `db:"num_nodes"`
		NumInstrumentedNodes    int32        `db:"num_instrumented_nodes"`
	}
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
	clusterUID := ""
	clusterName := ""
	clusterVersion := ""
	vizierVersion := ""

	if rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			log.WithError(err).Error("Could not query Vizier info")
			return nil, status.Error(codes.Internal, "could not query for viziers")
		}
		lastHearbeat := int64(-1)
		if val.LastHeartbeat != nil {
			lastHearbeat = *val.LastHeartbeat
		}

		if val.ClusterUID != nil {
			clusterUID = *val.ClusterUID
		}
		if val.ClusterName != nil {
			clusterName = *val.ClusterName
		}
		if val.ClusterVersion != nil {
			clusterVersion = *val.ClusterVersion
		}
		if val.VizierVersion != nil {
			vizierVersion = *val.VizierVersion
		}

		return &cvmsgspb.VizierInfo{
			VizierID:        utils.ProtoFromUUID(&val.ID),
			Status:          val.Status.ToProto(),
			LastHeartbeatNs: lastHearbeat,
			Config: &cvmsgspb.VizierConfig{
				PassthroughEnabled: val.PassthroughEnabled,
			},
			ClusterUID:              clusterUID,
			ClusterName:             clusterName,
			ClusterVersion:          clusterVersion,
			VizierVersion:           vizierVersion,
			ControlPlanePodStatuses: val.ControlPlanePodStatuses,
			NumNodes:                val.NumNodes,
			NumInstrumentedNodes:    val.NumInstrumentedNodes,
		}, nil
	}
	return nil, status.Error(codes.NotFound, "vizier not found")
}

// UpdateVizierConfig supports updating of the Vizier config.
func (s *Server) UpdateVizierConfig(ctx context.Context, req *cvmsgspb.UpdateVizierConfigRequest) (*cvmsgspb.UpdateVizierConfigResponse, error) {
	if err := s.validateOrgOwnsCluster(ctx, req.VizierID); err != nil {
		return nil, err
	}

	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)

	if req.ConfigUpdate == nil || req.ConfigUpdate.PassthroughEnabled == nil {
		return &cvmsgspb.UpdateVizierConfigResponse{}, nil
	}
	passthroughEnabled := req.ConfigUpdate.PassthroughEnabled.Value

	query := `
    UPDATE vizier_cluster_info
    SET passthrough_enabled = $1
    WHERE vizier_cluster_id = $2`

	res, err := s.db.Exec(query, passthroughEnabled, vizierID)
	if err != nil {
		return nil, err
	}
	if count, _ := res.RowsAffected(); count == 0 {
		return nil, status.Error(codes.NotFound, "no such cluster")
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
    SELECT vizier_cluster.id, vizier_cluster.org_id, vizier_cluster.cluster_uid, vizier_index_state.resource_version
    FROM vizier_cluster, vizier_cluster_info, vizier_index_state
    WHERE vizier_cluster_info.vizier_cluster_id=vizier_cluster.id
    	  AND vizier_cluster_info.vizier_cluster_id=vizier_index_state.cluster_id
    	  AND vizier_index_state.cluster_id=vizier_cluster.id
          AND vizier_cluster_info.status != 'DISCONNECTED'
          AND substring(vizier_cluster.id::text, 35)>=$1
          AND substring(vizier_cluster.id::text, 35)<=$2;`

	rows, err := s.db.Queryx(query, fromShardID, toShardID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	type Result struct {
		VizierID        uuid.UUID `db:"id"`
		OrgID           uuid.UUID `db:"org_id"`
		ResourceVersion string    `db:"resource_version"`
		K8sUID          string    `db:"cluster_uid"`
	}
	results := make([]*vzmgrpb.GetViziersByShardResponse_VizierInfo, 0)
	for rows.Next() {
		var result Result
		err = rows.StructScan(&result)
		if err != nil {

			return nil, status.Error(codes.Internal, "failed to read vizier info")
		}
		results = append(results, &vzmgrpb.GetViziersByShardResponse_VizierInfo{
			VizierID:        utils.ProtoFromUUID(&result.VizierID),
			OrgID:           utils.ProtoFromUUID(&result.OrgID),
			ResourceVersion: result.ResourceVersion,
			K8sUID:          result.K8sUID,
		})
	}

	return &vzmgrpb.GetViziersByShardResponse{Viziers: results}, nil
}

// VizierConnected is an the request made to the mgr to handle new Vizier connections.
func (s *Server) VizierConnected(ctx context.Context, req *cvmsgspb.RegisterVizierRequest) (*cvmsgspb.RegisterVizierAck, error) {
	log.WithField("req", req).Info("Received RegisterVizierRequest")

	vzVersion := ""
	clusterUID := ""

	if req.ClusterInfo != nil {
		vzVersion = req.ClusterInfo.VizierVersion
		clusterUID = req.ClusterInfo.ClusterUID
	}

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
    SET (last_heartbeat, address, jwt_signing_key, status, vizier_version)  = (
    	NOW(), $2, PGP_SYM_ENCRYPT($3, $4), $5, $6)
    WHERE vizier_cluster_id = $1`

	vzStatus := "CONNECTED"
	if req.Address == "" {
		vzStatus = "UNHEALTHY"
	}

	res, err := s.db.Exec(query, vizierID, req.Address, signingKey, s.dbKey, vzStatus, vzVersion)
	if err != nil {
		return nil, err
	}

	count, _ := res.RowsAffected()
	if count == 0 {
		return nil, status.Error(codes.NotFound, "no such cluster")
	}

	// Send a message over NATS to signal that a Vizier has connected.
	query = `SELECT org_id, resource_version from vizier_cluster AS c INNER JOIN vizier_index_state AS i ON c.id = i.cluster_id WHERE id=$1`
	var val struct {
		OrgID           uuid.UUID `db:"org_id"`
		ResourceVersion string    `db:"resource_version"`
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
		VizierID:        utils.ProtoFromUUID(&vizierID),
		ResourceVersion: val.ResourceVersion,
		OrgID:           utils.ProtoFromUUID(&val.OrgID),
		K8sUID:          clusterUID,
	}
	b, err := connMsg.Marshal()
	if err != nil {
		return nil, err
	}
	err = s.nc.Publish(messages.VizierConnectedChannel, b)
	if err != nil {
		return nil, err
	}
	return &cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_OK}, nil
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

	// Send DNS address.
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		log.WithError(err).Error("Could not get service creds from jwt")
		return
	}
	// TODO(michelle): fix
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
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

	// Fetch previous status.
	statusQuery := `SELECT i.status, i.vizier_version, c.cluster_name, c.cluster_version, c.org_id from vizier_cluster_info AS i INNER JOIN vizier_cluster as c ON c.id = i.vizier_cluster_id WHERE i.vizier_cluster_id=$1`
	var prevInfo struct {
		Status         string    `db:"status"`
		Version        string    `db:"vizier_version"`
		ClusterVersion string    `db:"cluster_version"`
		ClusterName    string    `db:"cluster_name"`
		OrgID          uuid.UUID `db:"org_id"`
	}
	rows, err := s.db.Queryx(statusQuery, vizierID)
	if err != nil {
		log.WithError(err).Error("Could not get current status")
	}
	defer rows.Close()
	if rows.Next() {
		err := rows.StructScan(&prevInfo)
		if err != nil {
			log.WithError(err).Error("Could not get current status")
		}
	}

	query := `
    UPDATE vizier_cluster_info
    SET last_heartbeat = NOW(), status = $1, address= $2, control_plane_pod_statuses= $3,
    	num_nodes = $4, num_instrumented_nodes = $5
    WHERE vizier_cluster_id = $6`

	vzStatus := "HEALTHY"
	if req.Address == "" {
		vzStatus = "UNHEALTHY"
	}
	if req.BootstrapMode {
		vzStatus = "UPDATING"
	}

	if req.Status != cvmsgspb.VZ_ST_UNKNOWN {
		s, err := vizierStatus(req.Status).Value()
		if err != nil {
			log.WithError(err).Error("Could not convert status")
			return
		}
		vzStatus = s.(string)
	}

	// Send cluster-level info.
	events.Client().Enqueue(&analytics.Track{
		UserId: vizierID.String(),
		Event:  events.VizierHeartbeat,
		Properties: analytics.NewProperties().
			Set("cluster_id", vizierID.String()).
			Set("status", vzStatus).
			Set("num_nodes", req.NumNodes).
			Set("num_instrumented_nodes", req.NumInstrumentedNodes).
			Set("cluster_name", prevInfo.ClusterName).
			Set("k8s_version", prevInfo.ClusterVersion).
			Set("vizier_version", prevInfo.Version).
			Set("org_id", prevInfo.OrgID.String()),
	})

	// Send analytics event for cluster status changes.
	if vzStatus != prevInfo.Status {
		events.Client().Enqueue(&analytics.Track{
			UserId: vizierID.String(),
			Event:  events.ClusterStatusChange,
			Properties: analytics.NewProperties().
				Set("cluster_id", vizierID.String()).
				Set("status", vzStatus),
		})
	}

	_, err = s.db.Exec(query, vzStatus, addr, PodStatuses(req.PodStatuses), req.NumNodes,
		req.NumInstrumentedNodes, vizierID)
	if err != nil {
		log.WithError(err).Error("Could not update vizier heartbeat")
	}
	if prevInfo.Status == "UPDATING" {
		return
	}

	if !req.BootstrapMode {
		if !s.updater.VersionUpToDate(prevInfo.Version) {
			s.updater.AddToUpdateQueue(vizierID)
		}
		return
	}

	// If we reach this point, the cluster is in bootstrap mode and needs to be deployed.
	go s.updater.UpdateOrInstallVizier(vizierID, req.BootstrapVersion, false)
}

// HandleSSLRequest registers certs for the vizier cluster.
func (s *Server) HandleSSLRequest(v2cMsg *cvmsgspb.V2CMessage) {
	anyMsg := v2cMsg.Msg

	req := &cvmsgspb.VizierSSLCertRequest{}
	err := types.UnmarshalAny(anyMsg, req)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal NATS message")
		return
	}

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		log.WithError(err).Error("Could not get creds from jwt")
		return
	}

	// TOOD(michelle): fix
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)

	dnsMgrReq := &dnsmgr.GetSSLCertsRequest{ClusterID: req.VizierID}
	resp, err := s.dnsMgrClient.GetSSLCerts(ctx, dnsMgrReq)
	if err != nil {
		log.WithError(err).Error("Could not get SSL certs")
		return
	}
	natsResp := &cvmsgspb.VizierSSLCertResponse{
		Key:  resp.Key,
		Cert: resp.Cert,
	}

	respAnyMsg, err := types.MarshalAny(natsResp)
	if err != nil {
		log.WithError(err).Error("Could not marshal proto to any")
		return
	}

	log.WithField("SSL", respAnyMsg.String()).Info("sending SSL response")
	s.sendNATSMessage("sslResp", respAnyMsg, vizierID)
}

// getServiceCredentials returns JWT credentials for inter-service requests.
func getServiceCredentials(signingKey string) (string, error) {
	claims := jwtutils.GenerateJWTForService("vzmgr Service")
	return jwtutils.SignJWTClaims(claims, signingKey)
}

// UpdateOrInstallVizier updates or installs the given vizier cluster to the specified version.
func (s *Server) UpdateOrInstallVizier(ctx context.Context, req *cvmsgspb.UpdateOrInstallVizierRequest) (*cvmsgspb.UpdateOrInstallVizierResponse, error) {
	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)

	v2cMsg, err := s.updater.UpdateOrInstallVizier(vizierID, req.Version, req.RedeployEtcd)

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

func setClusterNameIfNull(ctx context.Context, tx *sqlx.Tx, clusterID uuid.UUID, generateName func(i int) string) error {
	var existingName *string

	query := `SELECT cluster_name from vizier_cluster WHERE id=$1`
	err := tx.QueryRowxContext(ctx, query, clusterID).Scan(&existingName)
	if err != nil {
		return err
	}

	if existingName != nil {
		return nil
	}

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
		return errors.New("Could not find a unique cluster name")
	}

	query = `UPDATE vizier_cluster SET cluster_name=$1 WHERE id=$2`
	_, err = tx.ExecContext(ctx, query, finalName, clusterID)

	return err
}

// ProvisionOrClaimVizier provisions a given cluster or returns the ID if it already exists,
func (s *Server) ProvisionOrClaimVizier(ctx context.Context, orgID uuid.UUID, userID uuid.UUID, clusterUID string, clusterName string, clusterVersion string) (uuid.UUID, error) {
	// TODO(zasgar): This duplicates some functionality in the Create function. Will deprecate that Create function soon.
	tx, err := s.db.BeginTxx(ctx, nil)
	if err != nil {
		return uuid.Nil, vzerrors.ErrInternalDB
	}
	defer tx.Rollback()

	var clusterID uuid.UUID

	generateRandomName := func(i int) string {
		return namesgenerator.GetRandomName(i)
	}
	generateFromGivenName := func(i int) string {
		name := strings.TrimSpace(clusterName)
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

	assignNameAndCommit := func() (uuid.UUID, error) {
		generateNameFunc := generateRandomName
		if clusterName != "" {
			generateNameFunc = generateFromGivenName
		}

		if err := setClusterNameIfNull(ctx, tx, clusterID, generateNameFunc); err != nil {
			return uuid.Nil, vzerrors.ErrInternalDB
		}

		if err := tx.Commit(); err != nil {
			log.WithError(err).Error("Failed to commit transaction")
			return uuid.Nil, vzerrors.ErrInternalDB
		}
		return clusterID, nil
	}

	assignClusterVersion := func(clusterID uuid.UUID) error {
		query := `UPDATE vizier_cluster SET cluster_version=$1 WHERE id=$2`
		rows, err := tx.QueryxContext(ctx, query, clusterVersion, clusterID)
		if err != nil {
			return err
		}
		rows.Close()
		return nil
	}

	clusterID, status, err := findVizierWithUID(ctx, tx, orgID, clusterUID)
	if err != nil {
		return uuid.Nil, err
	}
	if clusterID != uuid.Nil {
		if status != vizierStatus(cvmsgspb.VZ_ST_DISCONNECTED) {
			return uuid.Nil, vzerrors.ErrProvisionFailedVizierIsActive
		}
		// Update cluster version.
		err = assignClusterVersion(clusterID)
		if err != nil {
			return uuid.Nil, err
		}

		return assignNameAndCommit()
	}

	clusterID, status, err = findVizierWithEmptyUID(ctx, tx, orgID)
	if err != nil {
		return uuid.Nil, err
	}
	if clusterID != uuid.Nil {
		// Set the cluster ID.
		query := `UPDATE vizier_cluster SET cluster_uid=$1 WHERE id=$2`
		rows, err := tx.QueryxContext(ctx, query, clusterUID, clusterID)
		if err != nil {
			return uuid.Nil, err
		}
		rows.Close()

		err = assignClusterVersion(clusterID)
		if err != nil {
			return uuid.Nil, err
		}
		return assignNameAndCommit()
	}

	// Insert new vizier case.
	query := `
    	WITH ins AS (
      		INSERT INTO vizier_cluster (org_id, project_name, cluster_uid, cluster_version) VALUES($1, $2, $3, $4) RETURNING id
		)
		INSERT INTO vizier_cluster_info(vizier_cluster_id, status) SELECT id, 'DISCONNECTED' FROM ins RETURNING vizier_cluster_id`
	err = tx.QueryRowContext(ctx, query, orgID, DefaultProjectName, clusterUID, clusterVersion).Scan(&clusterID)
	if err != nil {
		return uuid.Nil, err
	}

	// Create index state.
	query = `
		INSERT INTO vizier_index_state(cluster_id, resource_version) VALUES($1, '')
	`
	_, err = tx.ExecContext(ctx, query, &clusterID)
	if err != nil {
		log.WithError(err).Error("Failed to create vizier_index_state")
		return uuid.Nil, vzerrors.ErrInternalDB
	}
	return assignNameAndCommit()
}
