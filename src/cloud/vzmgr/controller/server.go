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
	dnsmgr "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/shared/messages"
	messagespb "pixielabs.ai/pixielabs/src/cloud/shared/messagespb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
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
}

// New creates a new server.
func New(db *sqlx.DB, dbKey string, dnsMgrClient dnsmgr.DNSMgrServiceClient, nc *nats.Conn) *Server {
	natsSubs := make([]*nats.Subscription, 0)
	natsCh := make(chan *nats.Msg)
	msgHandlerMap := make(map[string]HandleNATSMessageFunc)
	s := &Server{db, dbKey, dnsMgrClient, nc, natsCh, natsSubs, msgHandlerMap}

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
	if err := validateOrgID(ctx, req.OrgID); err != nil {
		return nil, err
	}
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	projectName := req.ProjectName
	if req.ProjectName == "" {
		projectName = DefaultProjectName
	}

	tx, err := s.db.BeginTxx(ctx, nil)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to create transaction")
	}
	defer tx.Rollback()

	// Note that we don't check whether the project name exists in the project table.
	// This was to avoid a dependency from the vzmgr service on the project manager service.
	// It is expected that the caller of CreateVizierCluster will first validate the project
	// name before invoking this API.

	query := `
    	WITH ins AS (
      		INSERT INTO vizier_cluster (org_id, project_name) VALUES($1, $2) RETURNING id
		)
		INSERT INTO vizier_cluster_info(vizier_cluster_id, status) SELECT id, 'DISCONNECTED'  FROM ins RETURNING vizier_cluster_id`
	row, err := s.db.Queryx(query, orgID, projectName)
	if err != nil {
		return nil, status.Errorf(codes.Internal, err.Error())
	}
	defer row.Close()

	var idPb *uuidpb.UUID
	var id uuid.UUID

	if row.Next() {
		if err := row.Scan(&id); err != nil {
			return nil, err
		}
		tx.Commit()

		idPb = utils.ProtoFromUUID(&id)
	} else {
		return nil, status.Error(codes.Internal, "failed to read cluster id")
	}

	// Create index state.
	query = `
		INSERT INTO vizier_index_state(cluster_id, resource_version) VALUES($1, '')
	`
	idxRow, err := s.db.Queryx(query, &id)
	if err != nil {
		return nil, status.Errorf(codes.Internal, err.Error())
	}
	defer idxRow.Close()

	return idPb, nil
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

	query := `SELECT vizier_cluster_id, status, (EXTRACT(EPOCH FROM age(now(), last_heartbeat))*1E9)::bigint as last_heartbeat,
              passthrough_enabled from vizier_cluster_info WHERE vizier_cluster_id=$1`
	var val struct {
		ID                 uuid.UUID    `db:"vizier_cluster_id"`
		Status             vizierStatus `db:"status"`
		LastHeartbeat      *int64       `db:"last_heartbeat"`
		PassthroughEnabled bool         `db:"passthrough_enabled"`
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
		lastHearbeat := int64(-1)
		if val.LastHeartbeat != nil {
			lastHearbeat = *val.LastHeartbeat
		}
		return &cvmsgspb.VizierInfo{
			VizierID:        utils.ProtoFromUUID(&val.ID),
			Status:          val.Status.ToProto(),
			LastHeartbeatNs: lastHearbeat,
			Config: &cvmsgspb.VizierConfig{
				PassthroughEnabled: val.PassthroughEnabled,
			},
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
	if len(req.ShardID) != 2 {
		return nil, status.Error(codes.InvalidArgument, "ShardID must be two hex digits")
	}
	if _, err := hex.DecodeString(req.ShardID); err != nil {
		return nil, status.Error(codes.InvalidArgument, "ShardID must be two hex digits")
	}
	shardID := strings.ToLower(req.ShardID)

	query := `
    SELECT vizier_cluster.id, vizier_cluster.org_id
    FROM vizier_cluster,vizier_cluster_info
    WHERE vizier_cluster_info.vizier_cluster_id=vizier_cluster.id
          AND vizier_cluster_info.status != 'DISCONNECTED'
          AND substring(vizier_cluster.id::text, 35)=$1;`

	rows, err := s.db.Queryx(query, shardID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	type Result struct {
		VizierID uuid.UUID `db:"id"`
		OrgID    uuid.UUID `db:"org_id"`
	}
	results := make([]*vzmgrpb.GetViziersByShardResponse_VizierInfo, 0)
	for rows.Next() {
		var result Result
		err = rows.StructScan(&result)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read vizier info")
		}
		results = append(results, &vzmgrpb.GetViziersByShardResponse_VizierInfo{
			VizierID: utils.ProtoFromUUID(&result.VizierID),
			OrgID:    utils.ProtoFromUUID(&result.OrgID),
		})
	}

	return &vzmgrpb.GetViziersByShardResponse{Viziers: results}, nil
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

	vizierID := utils.UUIDFromProtoOrNil(req.VizierID)
	query := `
    UPDATE vizier_cluster_info
    SET last_heartbeat = NOW(), status = $1, address= $2
    WHERE vizier_cluster_id = $3`

	vzStatus := "HEALTHY"
	if req.Address == "" {
		vzStatus = "UNHEALTHY"
	}

	_, err = s.db.Exec(query, vzStatus, addr, vizierID)
	if err != nil {
		log.WithError(err).Error("Could not update vizier heartbeat")
	}
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
