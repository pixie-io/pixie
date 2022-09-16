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

package bridge_test

import (
	"context"
	"fmt"
	"net"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sync/errgroup"
	"google.golang.org/genproto/googleapis/rpc/code"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/grpc/test/bufconn"

	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzconn/bridge"
	"px.dev/pixie/src/cloud/vzconn/vzconnpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	mock_vzmgrpb "px.dev/pixie/src/cloud/vzmgr/vzmgrpb/mock"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

const bufSize = 1024 * 1024

type testState struct {
	t   *testing.T
	lis *bufconn.Listener
	b   *bridge.GRPCServer

	nc               *nats.Conn
	conn             *grpc.ClientConn
	mockVZMgr        *mock_vzmgrpb.MockVZMgrServiceClient
	mockVZDeployment *mock_vzmgrpb.MockVZDeploymentServiceClient
}

func createTestState(t *testing.T, ctrl *gomock.Controller) (*testState, func(t *testing.T)) {
	viper.Set("jwt_signing_key", "jwtkey")
	lis := bufconn.Listen(bufSize)
	s := grpc.NewServer()
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	mockVZDeployment := mock_vzmgrpb.NewMockVZDeploymentServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	js := msgbus.MustConnectJetStream(nc)
	st, err := msgbus.NewJetStreamStreamer(nc, js, &nats.StreamConfig{
		Name:     "test",
		Subjects: []string{"test"},
		MaxAge:   1 * time.Minute,
	})
	require.NoError(t, err)
	b := bridge.NewBridgeGRPCServer(mockVZMgr, mockVZDeployment, nc, st)
	vzconnpb.RegisterVZConnServiceServer(s, b)

	eg := errgroup.Group{}
	eg.Go(func() error { return s.Serve(lis) })

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to dial bufnet: %v", err)
	}

	cleanupFunc := func(t *testing.T) {
		natsCleanup()
		conn.Close()

		// NATSBridgeController doesn't have a Stop method and blocks until it hits errors.
		// So we cannot gracefulStop the grpc server, that causes the test to hang.
		s.Stop()

		err := eg.Wait()
		if err != nil {
			t.Fatalf("failed to start server: %v", err)
		}
	}

	return &testState{
		t:                t,
		lis:              lis,
		b:                b,
		nc:               nc,
		conn:             conn,
		mockVZMgr:        mockVZMgr,
		mockVZDeployment: mockVZDeployment,
	}, cleanupFunc
}

func createDialer(lis *bufconn.Listener) func(ctx context.Context, url string) (net.Conn, error) {
	return func(ctx context.Context, url string) (net.Conn, error) {
		return lis.Dial()
	}
}

type readMsgWrapper struct {
	msg *vzconnpb.C2VBridgeMessage
	err error
}

func grpcReader(stream vzconnpb.VZConnService_NATSBridgeClient) chan readMsgWrapper {
	c := make(chan readMsgWrapper)
	go func() {
		for {
			select {
			case <-stream.Context().Done():
				return
			default:
				m, err := stream.Recv()
				c <- readMsgWrapper{
					msg: m,
					err: err,
				}
			}
		}
	}()
	return c
}

func convertToAny(msg proto.Message) *types.Any {
	anyMsg, err := types.MarshalAny(msg)
	if err != nil {
		panic(err)
	}
	return anyMsg
}

func TestNATSGRPCBridgeHandshakeTest_CorrectRegistration(t *testing.T) {
	ctrl := gomock.NewController(t)
	ts, cleanup := createTestState(t, ctrl)
	defer cleanup(t)

	vizierID := uuid.Must(uuid.NewV4())
	regReq := &cvmsgspb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil(vizierID.String()),
		JwtKey:   "123",
	}

	ts.mockVZMgr.EXPECT().
		VizierConnected(gomock.Any(), regReq).
		Return(&cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_OK}, nil)

	// Make some GRPC Requests.
	ctx := context.Background()
	client := vzconnpb.NewVZConnServiceClient(ts.conn)
	stream, err := client.NATSBridge(ctx)
	if err != nil {
		t.Fatal(err)
	}

	readCh := grpcReader(stream)

	err = stream.Send(&vzconnpb.V2CBridgeMessage{
		Topic:     "register",
		SessionId: 0,
		Msg:       convertToAny(regReq),
	})

	require.NoError(t, err)
	// Should get the register ACK.
	m := <-readCh
	assert.Nil(t, m.err)
	assert.Equal(t, "registerAck", m.msg.Topic)
	ack := cvmsgspb.RegisterVizierAck{}
	err = types.UnmarshalAny(m.msg.Msg, &ack)
	if err != nil {
		t.Fatal("Expected to get back RegisterVizierAck message")
	}

	assert.Equal(t, cvmsgspb.ST_OK, ack.Status)
}

func TestNATSGRPCBridgeHandshakeTest_MissingRegister(t *testing.T) {
	ctrl := gomock.NewController(t)
	ts, cleanup := createTestState(t, ctrl)
	defer cleanup(t)

	// Make some GRPC Requests.
	ctx := context.Background()
	client := vzconnpb.NewVZConnServiceClient(ts.conn)
	stream, err := client.NATSBridge(ctx)
	if err != nil {
		t.Fatal(err)
	}

	readCh := grpcReader(stream)
	err = stream.Send(&vzconnpb.V2CBridgeMessage{
		Topic:     "not-register",
		SessionId: 0,
		Msg:       nil,
	})
	require.NoError(t, err)

	// Should get the register error.
	m := <-readCh
	assert.NotNil(t, m.err)
	assert.NotNil(t, code.Code_INVALID_ARGUMENT, status.Code(m.err))
	assert.Nil(t, m.msg)
}

func TestNATSGRPCBridgeHandshakeTest_NilRegister(t *testing.T) {
	ctrl := gomock.NewController(t)
	ts, cleanup := createTestState(t, ctrl)
	defer cleanup(t)

	// Make some GRPC Requests.
	ctx := context.Background()
	client := vzconnpb.NewVZConnServiceClient(ts.conn)
	stream, err := client.NATSBridge(ctx)
	if err != nil {
		t.Fatal(err)
	}

	readCh := grpcReader(stream)
	err = stream.Send(&vzconnpb.V2CBridgeMessage{
		Topic:     "register",
		SessionId: 0,
		Msg:       nil,
	})
	require.NoError(t, err)

	// Should get the register error.
	m := <-readCh
	assert.NotNil(t, m.err)
	assert.NotNil(t, code.Code_INVALID_ARGUMENT, status.Code(m.err))
	assert.Nil(t, m.msg)
}

func TestNATSGRPCBridgeHandshakeTest_MalformedRegister(t *testing.T) {
	ctrl := gomock.NewController(t)
	ts, cleanup := createTestState(t, ctrl)
	defer cleanup(t)

	// Make some GRPC Requests.
	ctx := context.Background()
	client := vzconnpb.NewVZConnServiceClient(ts.conn)
	stream, err := client.NATSBridge(ctx)
	if err != nil {
		t.Fatal(err)
	}

	readCh := grpcReader(stream)
	err = stream.Send(&vzconnpb.V2CBridgeMessage{
		Topic:     "register",
		SessionId: 0,
		Msg:       convertToAny(&cvmsgspb.RegisterVizierAck{}),
	})
	require.NoError(t, err)

	// Should get the register error.
	m := <-readCh
	assert.NotNil(t, m.err)
	assert.NotNil(t, code.Code_INVALID_ARGUMENT, status.Code(m.err))
	assert.Nil(t, m.msg)
}

func registerVizier(ts *testState, vizierID uuid.UUID, stream vzconnpb.VZConnService_NATSBridgeClient, readCh chan readMsgWrapper) {
	regReq := &cvmsgspb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUIDStrOrNil(vizierID.String()),
		JwtKey:   "123",
	}

	ts.mockVZMgr.EXPECT().
		VizierConnected(gomock.Any(), regReq).
		Return(&cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_OK}, nil)

	err := stream.Send(&vzconnpb.V2CBridgeMessage{
		Topic:     "register",
		SessionId: 0,
		Msg:       convertToAny(regReq),
	})
	require.NoError(ts.t, err)

	// Should get the register ACK.
	m := <-readCh
	assert.Nil(ts.t, m.err)
	assert.Equal(ts.t, "registerAck", m.msg.Topic)
	ack := cvmsgspb.RegisterVizierAck{}
	err = types.UnmarshalAny(m.msg.Msg, &ack)
	if err != nil {
		ts.t.Fatal("Expected to get back RegisterVizierAck message")
	}

	assert.Equal(ts.t, cvmsgspb.ST_OK, ack.Status)
}

func TestNATSGRPCBridge_BridgingTest(t *testing.T) {
	ctrl := gomock.NewController(t)
	ts, cleanup := createTestState(t, ctrl)
	defer cleanup(t)

	// Make some GRPC Requests.
	ctx := context.Background()
	client := vzconnpb.NewVZConnServiceClient(ts.conn)
	stream, err := client.NATSBridge(ctx)
	if err != nil {
		t.Fatal(err)
	}

	readCh := grpcReader(stream)
	vizierID := uuid.Must(uuid.NewV4())
	registerVizier(ts, vizierID, stream, readCh)

	t1Ch := make(chan *nats.Msg, 10)
	topic := vzshard.V2CTopic("t1", vizierID)
	sub, err := ts.nc.ChanSubscribe(topic, t1Ch)
	require.NoError(t, err)
	defer sub.Unsubscribe()

	fmt.Printf("Listing to topic: %s\n", topic)
	// Send a message on t1 and expect it show up on t1Ch
	err = stream.Send(&vzconnpb.V2CBridgeMessage{
		Topic:     "t1",
		SessionId: 0,
		Msg: convertToAny(&cvmsgspb.VizierHeartbeat{
			VizierID: utils.ProtoFromUUIDStrOrNil(vizierID.String()),
		}),
	})
	require.NoError(t, err)

	natsMsg := <-t1Ch

	expectedMsg := &cvmsgspb.V2CMessage{
		VizierID: vizierID.String(),
		Msg: convertToAny(&cvmsgspb.VizierHeartbeat{
			VizierID: utils.ProtoFromUUIDStrOrNil(vizierID.String()),
		}),
	}

	msg := &cvmsgspb.V2CMessage{}
	err = msg.Unmarshal(natsMsg.Data)
	require.NoError(t, err)
	assert.Equal(t, expectedMsg, msg)
}

func TestNATSGRPCBridge_RegisterVizierDeployment(t *testing.T) {
	vizierID := uuid.Must(uuid.NewV4())
	ctrl := gomock.NewController(t)
	ts, cleanup := createTestState(t, ctrl)
	defer cleanup(t)

	ts.mockVZDeployment.EXPECT().
		RegisterVizierDeployment(gomock.Any(), &vzmgrpb.RegisterVizierDeploymentRequest{
			K8sClusterUID:  "test",
			DeploymentKey:  "deploy-key",
			K8sClusterName: "some name",
		}).
		Return(&vzmgrpb.RegisterVizierDeploymentResponse{VizierID: utils.ProtoFromUUID(vizierID), VizierName: "some_name"}, nil)

	// Make some GRPC Requests.
	ctx := context.Background()
	ctx = metadata.AppendToOutgoingContext(ctx, "X-API-KEY", "deploy-key")

	client := vzconnpb.NewVZConnServiceClient(ts.conn)
	resp, err := client.RegisterVizierDeployment(ctx, &vzconnpb.RegisterVizierDeploymentRequest{
		K8sClusterUID:  "test",
		K8sClusterName: "some name",
	})
	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(vizierID), resp.VizierID)
}
