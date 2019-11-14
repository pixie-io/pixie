package controllers

import (
	"context"
	"errors"
	"io"
	"time"

	"github.com/gogo/protobuf/types"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/utils"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
	cloud_connectorpb "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb"
)

const heartbeatIntervalS = 5 * time.Second
const heartbeatWaitS = 2 * time.Second

// VizierInfo fetches information about Vizier.
type VizierInfo interface {
	GetAddress() (string, int32, error)
}

// Server defines an gRPC server type.
type Server struct {
	vzConnClient  vzconnpb.VZConnServiceClient
	certMgrClient certmgrpb.CertMgrServiceClient
	vizierID      uuid.UUID
	jwtSigningKey string
	hbSeqNum      int64
	clock         utils.Clock
	quitCh        chan bool
	vzInfo        VizierInfo
	logCh         chan *cloud_connectorpb.TransferLogRequest
}

// NewServer creates GRPC handlers.
func NewServer(vizierID uuid.UUID, jwtSigningKey string, vzConnClient vzconnpb.VZConnServiceClient, certMgrClient certmgrpb.CertMgrServiceClient, vzInfo VizierInfo) *Server {
	clock := utils.SystemClock{}
	return NewServerWithClock(vizierID, jwtSigningKey, vzConnClient, certMgrClient, vzInfo, clock)
}

// NewServerWithClock creates a new server with the given clock.
func NewServerWithClock(vizierID uuid.UUID, jwtSigningKey string, vzConnClient vzconnpb.VZConnServiceClient, certMgrClient certmgrpb.CertMgrServiceClient, vzInfo VizierInfo, clock utils.Clock) *Server {
	return &Server{
		vizierID:      vizierID,
		jwtSigningKey: jwtSigningKey,
		vzConnClient:  vzConnClient,
		certMgrClient: certMgrClient,
		hbSeqNum:      0,
		clock:         clock,
		vzInfo:        vzInfo,
		quitCh:        make(chan bool),
		logCh:         make(chan *cloud_connectorpb.TransferLogRequest),
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

// RunStream manages starting and restarting the stream to VZConn.
func (s *Server) RunStream() {
	for {
		select {
		case <-s.quitCh:
			return
		default:
			log.Info("Starting stream")
			err := s.StartStream()
			if err == nil {
				log.Info("Stream ending")
				return
			}
			log.WithError(err).Error("Stream errored. Restarting stream")
			time.Sleep(heartbeatWaitS)
		}
	}
}

// StartStream starts the stream between the cloud connector and vizier connector.
func (s *Server) StartStream() error {
	stream, err := s.vzConnClient.CloudConnect(context.Background())
	if err != nil {
		log.WithError(err).Error("Error starting stream")
		return err
	}

	err = s.RegisterVizier(stream)
	if err != nil {
		log.WithError(err).Error("failed to register Vizier")
		return err
	}

	// Request the SSL certs and then send them cert manager.
	// TODO(zasgar/michelle): In the future we should update this so that the
	// cert manager is the one who initiates cert requests.
	err = s.RequestAndHandleSSLCerts(stream)
	if err != nil {
		log.WithError(err).Error("Failed to fetch SSL certs")
		return err
	}

	// Forward logs to vizier connector.
	go s.DoLogTransfer(stream)

	// Send heartbeats to vizier connector
	return s.DoHeartbeats(stream)
}

func wrapRequest(p *types.Any, topic string) *vzconnpb.CloudConnectRequest {
	return &vzconnpb.CloudConnectRequest{
		Topic: topic,
		Msg:   p,
	}
}

// RegisterVizier registers the cluster with VZConn.
func (s *Server) RegisterVizier(stream vzconnpb.VZConnService_CloudConnectClient) error {
	addr, _, err := s.vzInfo.GetAddress()
	if err != nil {
		log.WithError(err).Info("Unable to get vizier proxy address")
	}

	// Send over a registration request and wait for ACK.
	regReq := &cloudpb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUID(&s.vizierID),
		JwtKey:   s.jwtSigningKey,
		Address:  addr,
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

			switch registerAck.Status {
			case cloudpb.ST_FAILED_NOT_FOUND:
				return errors.New("registration not found, cluster unknown in pixie-cloud")
			case cloudpb.ST_OK:
				return nil
			default:
				return errors.New("registration unsuccessful: " + err.Error())
			}
		}, heartbeatWaitS)

		if err == nil {
			return nil // Registered successfully.
		}
		tries++
	}

	return err
}

// DoHeartbeats is responsible for executing the heartbeats.
func (s *Server) DoHeartbeats(stream vzconnpb.VZConnService_CloudConnectClient) error {
	for {
		select {
		case <-s.quitCh:
			return nil
		case <-time.Tick(heartbeatIntervalS * time.Second):
			err := s.HandleHeartbeat(stream)
			if err != nil {
				return err
			}
		}
	}
}

// DoLogTransfer is responsible for forwarding log messages to vzconn.
func (s *Server) DoLogTransfer(stream vzconnpb.VZConnService_CloudConnectClient) error {
	for {
		logMsg, chOpen := <-s.logCh
		if !chOpen {
			return nil
		}
		err := s.transferLogMsg(stream, logMsg)
		if err != nil {
			log.WithError(err).Error("Failed to perform log transfer")
			return err
		}
	}
}

// HandleHeartbeat sends a heartbeat to the VZConn and waits for a response.
func (s *Server) HandleHeartbeat(stream vzconnpb.VZConnService_CloudConnectClient) error {
	addr, port, err := s.vzInfo.GetAddress()
	if err != nil {
		log.WithError(err).Info("Unable to get vizier proxy address")
	}

	hbMsg := cloudpb.VizierHeartbeat{
		VizierID:       utils.ProtoFromUUID(&s.vizierID),
		Time:           s.clock.Now().Unix(),
		SequenceNumber: s.hbSeqNum,
		Address:        addr,
		Port:           port,
	}

	hbMsgAny, err := types.MarshalAny(&hbMsg)
	if err != nil {
		log.WithError(err).Info("Could not marshal heartbeat message")
		return err
	}

	// TODO(zasgar/michelle): There should be a vizier specific topic.
	msg := wrapRequest(hbMsgAny, "heartbeat")
	err = stream.Send(msg)

	if err == io.EOF {
		log.WithError(err).Info("Stream closed")
		return err
	}

	if err != nil {
		log.WithError(err).Info("Could not send heartbeat (will retry)")
		return nil
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

	return err
}

// RequestAndHandleSSLCerts registers the cluster with VZConn.
func (s *Server) RequestAndHandleSSLCerts(stream vzconnpb.VZConnService_CloudConnectClient) error {
	// Send over a request for SSL certs.
	regReq := &cloudpb.VizierSSLCertRequest{
		VizierID: utils.ProtoFromUUID(&s.vizierID),
	}
	anyMsg, err := types.MarshalAny(regReq)
	if err != nil {
		return err
	}
	wrappedReq := wrapRequest(anyMsg, "ssl")
	if err := stream.Send(wrappedReq); err != nil {
		return err
	}

	resp, err := stream.Recv()
	if err != nil {
		return err
	}

	sslCertResp := &cloudpb.VizierSSLCertResponse{}
	err = types.UnmarshalAny(resp.Msg, sslCertResp)
	if err != nil {
		return err
	}

	certMgrReq := &certmgrpb.UpdateCertsRequest{
		Key:  sslCertResp.Key,
		Cert: sslCertResp.Cert,
	}
	crtMgrResp, err := s.certMgrClient.UpdateCerts(stream.Context(), certMgrReq)
	if err != nil {
		return err
	}

	if !crtMgrResp.OK {
		return errors.New("Failed to update certs")
	}
	return nil
}

func (s *Server) transferLogMsg(stream vzconnpb.VZConnService_CloudConnectClient, logMsg *cloud_connectorpb.TransferLogRequest) error {
	anyMsg, err := types.MarshalAny(logMsg)
	if err != nil {
		log.WithError(err).WithField("message", logMsg).Errorf("Could not marshal log message")
		// We don't want the entire transfer log stream to fail on this case, so just log the error.
		return nil
	}
	wrappedReq := wrapRequest(anyMsg, "logs")
	// Vzconn does not respond to log messages
	if err := stream.Send(wrappedReq); err != nil {
		return err
	}
	return nil
}

// TransferLog is a method necessary to implement cloud_connectorpb's service
func (s *Server) TransferLog(stream cloud_connectorpb.CloudConnectorService_TransferLogServer) error {
	for {
		req, err := stream.Recv()
		if err == io.EOF {
			log.Info("Client log stream ending")
			return stream.SendAndClose(&cloud_connectorpb.TransferLogResponse{
				Ok: true,
			})
		} else if err != nil {
			log.WithError(err).Info("Received error from TransferLog stream")
			return err
		}
		s.logCh <- req
	}
}

// Stop stops the server and ends the heartbeats.
func (s *Server) Stop() {
	close(s.logCh)
	close(s.quitCh)
}
