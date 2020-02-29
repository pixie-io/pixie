package messageprocessor

import (
	"context"
	"errors"
	"fmt"

	"pixielabs.ai/pixielabs/src/shared/services/utils"

	"github.com/gogo/protobuf/types"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"
	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
)

// ErrorRegistrationFailedUnknown is the error for vizier registration failure.
var ErrorRegistrationFailedUnknown = errors.New("registration failed unknown")

// ErrorRegistrationFailedNotFound is the error for vizier registration failure when vizier is not found.
var ErrorRegistrationFailedNotFound = errors.New("registration failed not found")

// ErrorRequestChannelClosed is an error returned when the streams have already been closed.
var ErrorRequestChannelClosed = errors.New("request channel already closed")

// TODO(zasgar/michelle): Remove this, we need to make this into cluster credentials.
// getServiceCredentials returns JWT credentials for inter-service requests.
func getServiceCredentials(signingKey string) (string, error) {
	claims := utils.GenerateJWTForService("Vzconn Service")
	return utils.SignJWTClaims(claims, signingKey)
}

type requestMessage struct {
	msg *vzconnpb.CloudConnectRequest
}

type responseMessage struct {
	msg *vzconnpb.CloudConnectResponse
	err error
}

// MessageProcessor tracks a connection stream and routes it to the appropriate donwstream endpoint.
type MessageProcessor struct {
	ctx                context.Context
	streamID           uuid.UUID
	streamLog          *log.Entry
	requestCh          chan requestMessage
	responseCh         chan responseMessage
	quitCh             chan bool
	gotRegisterMessage bool
	vzmgrClient        vzmgrpb.VZMgrServiceClient
}

// NewMessageProcessor creates a new message processor.
func NewMessageProcessor(ctx context.Context, streamID uuid.UUID, vzmgrClient vzmgrpb.VZMgrServiceClient) *MessageProcessor {
	return &MessageProcessor{
		ctx:                ctx,
		streamID:           streamID,
		streamLog:          log.WithField("streamID", streamID),
		requestCh:          make(chan requestMessage),
		responseCh:         make(chan responseMessage),
		quitCh:             make(chan bool),
		gotRegisterMessage: false,
		vzmgrClient:        vzmgrClient,
	}
}

func (m *MessageProcessor) handleRegisterMessage(msg *cloudpb.RegisterVizierRequest) error {
	m.streamLog.WithField("vizier_id", msg.VizierID).
		Info("Vizier registration request")

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return err
	}
	ctx := metadata.AppendToOutgoingContext(m.ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))
	vzmgrResp, err := m.vzmgrClient.VizierConnected(ctx, msg)
	if err != nil {
		return err
	}

	var respAsAny *types.Any
	if respAsAny, err = types.MarshalAny(vzmgrResp); err != nil {
		return err
	}

	m.sendMessage(&vzconnpb.CloudConnectResponse{
		Topic: "register",
		Msg:   respAsAny,
	}, nil)

	// If registration failed it's an error and we should destroy the stream processor.
	if vzmgrResp.Status == cloudpb.ST_OK {
		return nil
	}
	if vzmgrResp.Status == cloudpb.ST_FAILED_NOT_FOUND {
		return ErrorRegistrationFailedNotFound
	}
	return ErrorRegistrationFailedUnknown
}

func (m *MessageProcessor) handleVizierHeartbeat(msg *cloudpb.VizierHeartbeat) error {
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return err
	}
	ctx := metadata.AppendToOutgoingContext(m.ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	vzmgrResp, err := m.vzmgrClient.HandleVizierHeartbeat(ctx, msg)
	if err != nil {
		return err
	}

	var respAsAny *types.Any
	if respAsAny, err = types.MarshalAny(vzmgrResp); err != nil {
		return err
	}

	m.sendMessage(&vzconnpb.CloudConnectResponse{
		// TODO(zasgar/michelle): Should this be on a vizier specific channel? Need to change this when we move to pubsub model.
		Topic: "heartbeat",
		Msg:   respAsAny,
	}, nil)
	return nil
}

func (m *MessageProcessor) handleSSLCertMessage(msg *cloudpb.VizierSSLCertRequest) error {
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return err
	}
	ctx := metadata.AppendToOutgoingContext(m.ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	rpcReq := &vzmgrpb.GetSSLCertsRequest{ClusterID: msg.VizierID}
	vzmgrResp, err := m.vzmgrClient.GetSSLCerts(ctx, rpcReq)
	if err != nil {
		return err
	}

	ccResp := &cloudpb.VizierSSLCertResponse{
		Key:  vzmgrResp.Key,
		Cert: vzmgrResp.Cert,
	}
	var respAsAny *types.Any
	if respAsAny, err = types.MarshalAny(ccResp); err != nil {
		return err
	}

	m.sendMessage(&vzconnpb.CloudConnectResponse{
		// TODO(zasgar/michelle): Should this be on a vizier specific channel? Need to change this when we move to pubsub model.
		Topic: "ssl",
		Msg:   respAsAny,
	}, nil)

	return nil
}

func (m *MessageProcessor) handleMessage(msg *vzconnpb.CloudConnectRequest) error {
	dynamicMsg := types.DynamicAny{}
	err := types.UnmarshalAny(msg.Msg, &dynamicMsg)
	if err != nil {
		return err
	}

	if !m.gotRegisterMessage {
		if _, ok := dynamicMsg.Message.(*cloudpb.RegisterVizierRequest); !ok {
			m.streamLog.Errorf("Expected registration message as the first message, got: %s", msg.Msg.TypeUrl)
			return errors.New("expected VizierRegistrationMessage")
		}
		m.gotRegisterMessage = true
	}

	switch msg := dynamicMsg.Message.(type) {
	case *cloudpb.RegisterVizierRequest:
		err = m.handleRegisterMessage(msg)
	case *cloudpb.VizierHeartbeat:
		// TODO(zasgar/michelle): move this to a channel.
		err = m.handleVizierHeartbeat(msg)
	case *cloudpb.VizierSSLCertRequest:
		err = m.handleSSLCertMessage(msg)
	default:
		m.streamLog.
			WithField("msg", msg.String()).
			Error("Unhandled message type")
	}

	if err != nil {
		m.streamLog.WithError(err).Error("Terminating stream")
		return err
	}
	return nil
}

// Run starts the message processing and is normally run as a goroutine.
func (m *MessageProcessor) Run() error {
	for {
		select {
		case msg := <-m.requestCh:
			{
				err := m.handleMessage(msg.msg)
				if err != nil {
					return err
				}
			}
		case <-m.quitCh:
			return nil
		}
	}
}

// Stop closes the message processor.
func (m *MessageProcessor) Stop() {
	log.Info("Stop called")
	if m.quitCh != nil {
		close(m.quitCh)
	}
	m.quitCh = nil
}

func (m *MessageProcessor) sendMessage(msg *vzconnpb.CloudConnectResponse, err error) {
	m.responseCh <- responseMessage{msg, err}
}

// ProcessMessage enqueues the passed in message for processing.
func (m *MessageProcessor) ProcessMessage(request *vzconnpb.CloudConnectRequest) error {
	select {
	case <-m.quitCh:
		return errors.New("request channel already closed")
	default:
	}
	m.requestCh <- requestMessage{request}
	return nil
}

// GetOutgoingMessage will get message that need to be sent back to the stream.
func (m *MessageProcessor) GetOutgoingMessage() (*vzconnpb.CloudConnectResponse, error) {
	select {
	case resp := <-m.responseCh:
		return resp.msg, resp.err
	case <-m.quitCh:
		return nil, nil
	}
}
