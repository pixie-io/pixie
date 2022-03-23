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

package bridge

import (
	"context"
	"fmt"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/vzconn/vzconnpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/shared/services/utils"
	utils2 "px.dev/pixie/src/utils"
)

var (
	bridgeMetricsCollector = &natsBridgeMetricCollector{}
)

func init() {
	prometheus.MustRegister(bridgeMetricsCollector)
}

// GRPCServer is implementation of the vzconn server.
type GRPCServer struct {
	vzmgrClient        vzmgrpb.VZMgrServiceClient
	vzDeploymentClient vzmgrpb.VZDeploymentServiceClient
	nc                 *nats.Conn
	st                 msgbus.Streamer
}

// NewBridgeGRPCServer creates a new GRPCServer.
func NewBridgeGRPCServer(vzmgrClient vzmgrpb.VZMgrServiceClient, vzDeploymentClient vzmgrpb.VZDeploymentServiceClient, nc *nats.Conn, st msgbus.Streamer) *GRPCServer {
	return &GRPCServer{vzmgrClient, vzDeploymentClient, nc, st}
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
		K8sClusterUID:  req.K8sClusterUID,
		K8sClusterName: req.K8sClusterName,
		DeploymentKey:  deployKey,
	})
	if err != nil {
		return nil, err
	}

	return &vzconnpb.RegisterVizierDeploymentResponse{
		VizierID:   vzmgrResp.VizierID,
		VizierName: vzmgrResp.VizierName,
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
	c := NewNATSBridgeController(utils2.UUIDFromProtoOrNil(clusterID), srv, s.nc, s.st)
	bridgeMetricsCollector.Register(c)
	defer bridgeMetricsCollector.Unregister(c)

	return convertToGRPCErr(c.Run())
}

func (s *GRPCServer) handleRegisterMessage(msg *cvmsgspb.RegisterVizierRequest, srv vzconnpb.VZConnService_NATSBridgeServer) error {
	vzID := utils2.UUIDFromProtoOrNil(msg.VizierID)

	log.WithField("VizierID", vzID.String()).
		Info("Vizier registration request")

	serviceAuthToken, err := getClusterCredentials(viper.GetString("jwt_signing_key"), vzID)
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

func getServiceCredentials(signingKey string) (string, error) {
	claims := utils.GenerateJWTForService("Vzconn Service", viper.GetString("domain_name"))
	return utils.SignJWTClaims(claims, signingKey)
}

func getClusterCredentials(signingKey string, clusterID uuid.UUID) (string, error) {
	claims := utils.GenerateJWTForCluster(clusterID.String(), viper.GetString("domain_name"))
	return utils.SignJWTClaims(claims, signingKey)
}
