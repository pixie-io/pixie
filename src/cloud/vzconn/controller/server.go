package controller

import (
	"context"
	"io"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/cloud/vzconn/messageprocessor"

	uuid "github.com/satori/go.uuid"
	"golang.org/x/sync/errgroup"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
)

type messageProcessor interface {
	Run() error
	Stop()
	ProcessMessage(*vzconnpb.CloudConnectRequest) error
	GetOutgoingMessage() (*vzconnpb.CloudConnectResponse, error)
}

// Server is implementation of the vzconn server.
type Server struct {
	vzmgrClient vzmgrpb.VZMgrServiceClient
}

// NewServer creates a new Server.
func NewServer(vzmgrClient vzmgrpb.VZMgrServiceClient) *Server {
	return &Server{vzmgrClient: vzmgrClient}
}

func (s *Server) getMessage(ctx context.Context, streamLog *log.Entry, srv vzconnpb.VZConnService_CloudConnectServer) (*vzconnpb.CloudConnectRequest, error) {
	req, err := srv.Recv()
	if err == io.EOF {
		streamLog.Info("stream closed")
		return nil, nil
	}

	if err != nil {
		streamLog.WithError(err).Error("stream closed")
		return nil, err
	}
	return req, nil
}

// CloudConnectWithProcessor is the inner function of CloudConnect that takes the stream id and processor as aruguments. Used for testing.
func (s *Server) CloudConnectWithProcessor(streamID uuid.UUID, processor messageProcessor, srv vzconnpb.VZConnService_CloudConnectServer) error {
	streamLog := log.WithField("streamID", streamID)
	streamLog.Info("starting new cloud connect stream")
	ctx := srv.Context()

	streamWriter := func() error {
		defer processor.Stop()
		for {
			resp, err := processor.GetOutgoingMessage()
			if err != nil {
				return err
			}
			// Stream terminated.
			if resp == nil {
				return nil
			}
			err = srv.Send(resp)
			if err != nil {
				return err
			}
		}
	}

	streamReader := func() error {
		defer processor.Stop()
		for {
			select {
			case <-ctx.Done():
				return ctx.Err()
			default:
			}

			req, err := s.getMessage(ctx, streamLog, srv)
			if err != nil {
				return err
			}
			if req == nil {
				return nil
			}

			err = processor.ProcessMessage(req)
			if err != nil {
				return err
			}
		}
	}
	var g errgroup.Group
	g.Go(processor.Run)
	g.Go(streamWriter)
	g.Go(streamReader)

	err := g.Wait()
	if err != nil {
		streamLog.WithError(err).Error("Terminating stream")
	} else {
		streamLog.Info("Terminating stream")
	}
	return convertToGRPCErr(err)
}

// CloudConnect is the endpoint that all viziers connect to.
func (s *Server) CloudConnect(srv vzconnpb.VZConnService_CloudConnectServer) error {
	streamID := uuid.NewV4()
	streamLog := log.WithField("streamID", streamID)

	streamLog.Info("starting new cloud connect stream")
	ctx := srv.Context()

	processor := messageprocessor.NewMessageProcessor(ctx, streamID, s.vzmgrClient)
	defer processor.Stop()

	return s.CloudConnectWithProcessor(streamID, processor, srv)
}

func convertToGRPCErr(err error) error {
	if err == nil {
		return err
	}
	switch err {
	case messageprocessor.ErrorRegistrationFailedNotFound:
		return status.Error(codes.NotFound, err.Error())
	case context.Canceled:
		return status.Error(codes.Canceled, err.Error())
	case messageprocessor.ErrorRequestChannelClosed:
		return status.Error(codes.Canceled, err.Error())
	default:
		return status.Error(codes.Unknown, err.Error())
	}
}
