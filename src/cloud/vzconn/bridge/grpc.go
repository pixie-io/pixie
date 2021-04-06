package bridge

import (
	"context"
	"fmt"

	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	utils2 "pixielabs.ai/pixielabs/src/utils"
)

// GRPCServer is implementation of the vzconn server.
type GRPCServer struct {
	vzmgrClient        vzmgrpb.VZMgrServiceClient
	vzDeploymentClient vzmgrpb.VZDeploymentServiceClient
	nc                 *nats.Conn
	sc                 stan.Conn
}

// NewBridgeGRPCServer creates a new GRPCServer.
func NewBridgeGRPCServer(vzmgrClient vzmgrpb.VZMgrServiceClient, vzDeploymentClient vzmgrpb.VZDeploymentServiceClient, nc *nats.Conn, sc stan.Conn) *GRPCServer {
	return &GRPCServer{vzmgrClient, vzDeploymentClient, nc, sc}
}

// RegisterVizierDeployment registers the vizier using the deployment key passed in on X-API-KEY.
func (s *GRPCServer) RegisterVizierDeployment(ctx context.Context, req *vzconnpb.RegisterVizierDeploymentRequest) (*vzconnpb.RegisterVizierDeploymentResponse, error) {
	// Get the deploy key from the ctx.
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return nil, status.Error(codes.NotFound, "Could not get metadata from incoming md")
	}
	apiKeys := md["x-api-key"]
	if len(apiKeys) == 0 {
		return nil, status.Error(codes.Unauthenticated, "Deploy key not included")
	}

	deployKey := apiKeys[0]

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}
	newCtx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))
	vzmgrResp, err := s.vzDeploymentClient.RegisterVizierDeployment(newCtx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID:     req.K8sClusterUID,
		K8sClusterName:    req.K8sClusterName,
		K8sClusterVersion: req.K8sClusterVersion,
		DeploymentKey:     deployKey,
	})
	if err != nil {
		return nil, err
	}

	return &vzconnpb.RegisterVizierDeploymentResponse{
		VizierID: vzmgrResp.VizierID,
	}, nil
}

// NATSBridge is the endpoint that all viziers connect to.
func (s *GRPCServer) NATSBridge(srv vzconnpb.VZConnService_NATSBridgeServer) error {
	msg, err := srv.Recv()
	if err != nil {
		return convertToGRPCErr(err)
	}

	// We expect register message to be the first.
	if msg.Topic != "register" {
		return convertToGRPCErr(ErrMissingRegistrationMessage)
	}

	registerMsg := &cvmsgspb.RegisterVizierRequest{}
	err = types.UnmarshalAny(msg.Msg, registerMsg)
	if err != nil {
		return convertToGRPCErr(ErrBadRegistrationMessage)
	}
	clusterID := registerMsg.VizierID
	err = s.handleRegisterMessage(registerMsg, srv)
	if err != nil {
		return convertToGRPCErr(err)
	}

	// Registration successful. We are ready to start the bridge.

	// Each Vizier calls this endpoint. Once it's called we will basically
	// create NATS bridge and subscribe to the relevant channels.
	c := NewNATSBridgeController(utils2.UUIDFromProtoOrNil(clusterID), srv, s.nc, s.sc)
	return convertToGRPCErr(c.Run())
}

func (s *GRPCServer) handleRegisterMessage(msg *cvmsgspb.RegisterVizierRequest, srv vzconnpb.VZConnService_NATSBridgeServer) error {
	log.WithField("VizierID", utils2.UUIDFromProtoOrNil(msg.VizierID).String()).
		Info("Vizier registration request")

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return err
	}
	ctx := metadata.AppendToOutgoingContext(srv.Context(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))
	vzmgrResp, err := s.vzmgrClient.VizierConnected(ctx, msg)
	if err != nil {
		return err
	}

	var respAsAny *types.Any
	if respAsAny, err = types.MarshalAny(vzmgrResp); err != nil {
		return err
	}

	sendErr := srv.Send(&vzconnpb.C2VBridgeMessage{
		Topic: "registerAck",
		Msg:   respAsAny,
	})
	// If registration failed it's an error and we should destroy the stream processor.
	if vzmgrResp.Status == cvmsgspb.ST_OK {
		return sendErr
	}
	if vzmgrResp.Status == cvmsgspb.ST_FAILED_NOT_FOUND {
		return ErrRegistrationFailedNotFound
	}
	return ErrRegistrationFailedUnknown
}

func convertToGRPCErr(err error) error {
	if err == nil {
		return err
	}
	switch err {
	case context.Canceled:
		return status.Error(codes.Canceled, err.Error())
	case ErrMissingRegistrationMessage:
	case ErrBadRegistrationMessage:
		return status.Error(codes.InvalidArgument, err.Error())
	case ErrRegistrationFailedUnknown:
		return status.Error(codes.Unknown, err.Error())
	case ErrRegistrationFailedNotFound:
		return status.Error(codes.NotFound, err.Error())
	case ErrRequestChannelClosed:
		return status.Error(codes.Canceled, err.Error())
	}
	return status.Error(codes.Unknown, err.Error())
}

// TODO(zasgar/michelle): Remove this, we need to make this into cluster credentials.
// getServiceCredentials returns JWT credentials for inter-service requests.
func getServiceCredentials(signingKey string) (string, error) {
	claims := utils.GenerateJWTForService("Vzconn Service", viper.GetString("domain_name"))
	return utils.SignJWTClaims(claims, signingKey)
}
