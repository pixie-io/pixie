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

// Server defines an gRPC server type.
type Server struct {
	vzConnClient  vzconnpb.VZConnServiceClient
	vizierID      uuid.UUID
	jwtSigningKey string
}

// NewServer creates GRPC handlers.
func NewServer(vizierID uuid.UUID, jwtSigningKey string, vzConnClient vzconnpb.VZConnServiceClient) *Server {
	return &Server{
		vizierID:      vizierID,
		jwtSigningKey: jwtSigningKey,
		vzConnClient:  vzConnClient,
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

	err = s.registerVizier(stream)
	if err != nil {
		log.WithError(err).Fatal("Failed to register vizier")
	}
}

func wrapRequest(p *types.Any, topic string) *vzconnpb.CloudConnectRequest {
	return &vzconnpb.CloudConnectRequest{
		Topic: topic,
		Msg:   p,
	}
}

func (s *Server) registerVizier(stream vzconnpb.VZConnService_CloudConnectClient) error {
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
		}, 2*time.Second)

		if err == nil {
			return nil // Registered successfully.
		}
		tries++
	}

	return err
}
