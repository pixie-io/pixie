package controllers

import (
	"context"
	"errors"
	"time"

	"github.com/gogo/protobuf/types"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/utils"
)

const heartbeatIntervalS = 30 * time.Second
const heartbeatWaitS = 5 * time.Second

// Server defines an gRPC server type.
type Server struct {
	vzConnClient  vzconnpb.VZConnServiceClient
	vizierID      uuid.UUID
	jwtSigningKey string
	hbSeqNum      int64
	clock         utils.Clock
	quitCh        chan bool
}

// NewServer creates GRPC handlers.
func NewServer(vizierID uuid.UUID, jwtSigningKey string, vzConnClient vzconnpb.VZConnServiceClient) *Server {
	clock := utils.SystemClock{}
	return NewServerWithClock(vizierID, jwtSigningKey, vzConnClient, clock)
}

// NewServerWithClock creates a new server with the given clock.
func NewServerWithClock(vizierID uuid.UUID, jwtSigningKey string, vzConnClient vzconnpb.VZConnServiceClient, clock utils.Clock) *Server {
	return &Server{
		vizierID:      vizierID,
		jwtSigningKey: jwtSigningKey,
		vzConnClient:  vzConnClient,
		hbSeqNum:      0,
		clock:         clock,
		quitCh:        make(chan bool),
	}
}

// DoWithTimeout runs f and returns its error.  If the deadline d elapses first,
// it returns a grpc DeadlineExceeded error instead.
func DoWithTimeout(f func() error, d time.Duration) error {
	errChan := make(chan error, 1)
	go func() {
		errChan <- f()
		close(errChan)
	}()
	t := time.NewTimer(d)
	select {
	case <-t.C:
		return errors.New("timeout")
	case err := <-errChan:
		if !t.Stop() {
			<-t.C
		}
		return err
	}
}

// StartStream starts the stream between the cloud connector and vizier connector.
func (s *Server) StartStream() {
	stream, err := s.vzConnClient.CloudConnect(context.Background())
	if err != nil {
		log.WithError(err).Fatal("Could not start stream")
	}

	err = s.RegisterVizier(stream)
	if err != nil {
		log.WithError(err).Fatal("Failed to register vizier")
	}

	// Send heartbeats.
	go s.doHeartbeats(stream)
}

func wrapRequest(p *types.Any, topic string) *vzconnpb.CloudConnectRequest {
	return &vzconnpb.CloudConnectRequest{
		Topic: topic,
		Msg:   p,
	}
}

// RegisterVizier registers the cluster with VZConn.
func (s *Server) RegisterVizier(stream vzconnpb.VZConnService_CloudConnectClient) error {
	// Send over a registration request and wait for ACK.
	regReq := &cloudpb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUID(&s.vizierID),
		JwtKey:   s.jwtSigningKey,
	}

	anyMsg, err := types.MarshalAny(regReq)
	if err != nil {
		return err
	}
	wrappedReq := wrapRequest(anyMsg, "register")
	if err := stream.Send(wrappedReq); err != nil {
		return err
	}

	tries := 0
	for tries < 5 {
		err = DoWithTimeout(func() error {
			// Try to receive the registerAck.
			resp, err := stream.Recv()
			if err != nil {
				return err
			}
			registerAck := &cloudpb.RegisterVizierAck{}
			err = types.UnmarshalAny(resp.Msg, registerAck)
			if err != nil {
				return err
			}

			if registerAck.Status != cloudpb.ST_OK {
				return errors.New("vizier registration unsuccessful")
			}
			return nil
		}, heartbeatWaitS)

		if err == nil {
			return nil // Registered successfully.
		}
		tries++
	}

	return err
}

func (s *Server) doHeartbeats(stream vzconnpb.VZConnService_CloudConnectClient) {
	for {
		select {
		case <-s.quitCh:
			return
		default:
			s.HandleHeartbeat(stream)
			time.Sleep(heartbeatIntervalS)
		}
	}
}

// HandleHeartbeat sends a heartbeat to the VZConn and waits for a response.
func (s *Server) HandleHeartbeat(stream vzconnpb.VZConnService_CloudConnectClient) {
	hbMsg := cloudpb.VizierHeartbeat{
		VizierID:       utils.ProtoFromUUID(&s.vizierID),
		Time:           s.clock.Now().Unix(),
		SequenceNumber: s.hbSeqNum,
	}

	hbMsgAny, err := types.MarshalAny(&hbMsg)
	if err != nil {
		log.WithError(err).Fatal("Could not marshal heartbeat message")
	}

	// TODO(zasgar/michelle): There should be a vizier specific topic.
	msg := wrapRequest(hbMsgAny, "heartbeat")
	if err := stream.Send(msg); err != nil {
		log.WithError(err).Fatal("Could not send heartbeat (will retry)")
		return
	}

	s.hbSeqNum++

	err = DoWithTimeout(func() error {
		resp, err := stream.Recv()
		if err != nil {
			return errors.New("Could not receive heartbeat ack")
		}
		hbAck := &cloudpb.VizierHeartbeatAck{}
		err = types.UnmarshalAny(resp.Msg, hbAck)
		if err != nil {
			return errors.New("Could not unmarshal heartbeat ack")
		}

		if hbAck.SequenceNumber != hbMsg.SequenceNumber {
			return errors.New("Received out of sequence heartbeat ack")
		}
		return nil
	}, heartbeatWaitS)

	if err != nil {
		log.WithError(err).Fatal("Did not receive heartbeat ack")
	}
}

// Stop stops the server and ends the heartbeats.
func (s *Server) Stop() {
	close(s.quitCh)
}
