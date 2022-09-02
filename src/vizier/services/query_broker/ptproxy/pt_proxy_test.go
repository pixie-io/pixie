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

package ptproxy_test

import (
	"context"
	"errors"
	"net"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/test/bufconn"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/query_broker/ptproxy"
)

const (
	bufSize        = 1024 * 1024
	defaultTimeout = 30 * time.Second
)

type MockVzServer struct {
	t *testing.T
}

func NewMockVzServer(t *testing.T) *MockVzServer {
	return &MockVzServer{t}
}

func (m *MockVzServer) ExecuteScript(req *vizierpb.ExecuteScriptRequest, srv vizierpb.VizierService_ExecuteScriptServer) error {
	if req.QueryStr == "should pass" {
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: "1",
		}
		return srv.Send(resp)
	}
	if req.QueryStr == "error" {
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: "2",
		}
		err := srv.Send(resp)
		if err != nil {
			return err
		}
		return errors.New("Failed")
	}
	if req.QueryStr == "cancel" {
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: "3",
		}
		err := srv.Send(resp)
		if err != nil {
			return err
		}
		<-time.After(defaultTimeout)
		return errors.New("Timeout waiting for context to be canceled")
	}
	return nil
}

func (m *MockVzServer) HealthCheck(req *vizierpb.HealthCheckRequest, srv vizierpb.VizierService_HealthCheckServer) error {
	return nil
}

func (m *MockVzServer) GenerateOTelScript(ctx context.Context, req *vizierpb.GenerateOTelScriptRequest) (*vizierpb.GenerateOTelScriptResponse, error) {
	if req.PxlScript == "status_error" {
		return &vizierpb.GenerateOTelScriptResponse{
			Status: &vizierpb.Status{
				Code:    int32(codes.InvalidArgument),
				Message: "invalid argument",
			},
		}, nil
	}
	if req.PxlScript == "error" {
		return nil, errors.New("Failed")
	}
	// Default to success.
	return &vizierpb.GenerateOTelScriptResponse{
		Status:     &vizierpb.Status{},
		OTelScript: req.PxlScript,
	}, nil
}

type testState struct {
	t        *testing.T
	lis      *bufconn.Listener
	vzServer *MockVzServer

	nc   *nats.Conn
	conn *grpc.ClientConn
}

func createTestState(t *testing.T) (*testState, func(t *testing.T)) {
	lis := bufconn.Listen(bufSize)
	s := grpc.NewServer()

	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	vzServer := NewMockVzServer(t)
	vizierpb.RegisterVizierServiceServer(s, vzServer)

	eg := errgroup.Group{}
	eg.Go(func() error { return s.Serve(lis) })

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		natsCleanup()
		t.Fatalf("Failed to dial bufnet: %v", err)
	}

	cleanupFunc := func(t *testing.T) {
		natsCleanup()
		conn.Close()
		s.Stop()

		err := eg.Wait()
		if err != nil {
			t.Fatalf("failed to start server: %v", err)
		}
	}

	return &testState{
		t:        t,
		lis:      lis,
		vzServer: vzServer,
		nc:       nc,
		conn:     conn,
	}, cleanupFunc
}

func createDialer(lis *bufconn.Listener) func(ctx context.Context, url string) (net.Conn, error) {
	return func(ctx context.Context, url string) (net.Conn, error) {
		return lis.Dial()
	}
}

// unknownC2VMsg is used to test that unknown messages throw errors.
type unknownC2VMsg struct {
	cvmsgspb.C2VAPIStreamRequest_ExecReq
}

func (u *unknownC2VMsg) Equal(interface{}) bool        { return false }
func (u *unknownC2VMsg) MarshalTo([]byte) (int, error) { return 0, nil }
func (u *unknownC2VMsg) Size() int                     { return 0 }

func TestPassThroughProxy(t *testing.T) {
	tests := []struct {
		name          string
		requestID     string
		request       *cvmsgspb.C2VAPIStreamRequest
		expectedResps []*cvmsgspb.V2CAPIStreamResponse
		sendCancel    bool
	}{
		{
			name:      "complete",
			requestID: "1",
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &cvmsgspb.C2VAPIStreamRequest_ExecReq{
					ExecReq: &vizierpb.ExecuteScriptRequest{
						QueryStr: "should pass",
					},
				},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
						ExecResp: &vizierpb.ExecuteScriptResponse{
							QueryID: "1",
						},
					},
				},
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &vizierpb.Status{
							Code: int32(codes.OK),
						},
					},
				},
			},
		},
		{
			name:      "error",
			requestID: "2",
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &cvmsgspb.C2VAPIStreamRequest_ExecReq{
					ExecReq: &vizierpb.ExecuteScriptRequest{
						QueryStr: "error",
					},
				},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "2",
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
						ExecResp: &vizierpb.ExecuteScriptResponse{
							QueryID: "2",
						},
					},
				},
				{
					RequestID: "2",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &vizierpb.Status{
							Code:    int32(codes.Unknown),
							Message: "rpc error: code = Unknown desc = Failed",
						},
					},
				},
			},
		},
		{
			name:       "cancel",
			requestID:  "3",
			sendCancel: true,
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &cvmsgspb.C2VAPIStreamRequest_ExecReq{
					ExecReq: &vizierpb.ExecuteScriptRequest{
						QueryStr: "cancel",
					},
				},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "3",
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
						ExecResp: &vizierpb.ExecuteScriptResponse{
							QueryID: "3",
						},
					},
				},
			},
		},
		{
			name:      "otel script: success",
			requestID: "1",
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &cvmsgspb.C2VAPIStreamRequest_GenerateOTelScriptReq{
					GenerateOTelScriptReq: &vizierpb.GenerateOTelScriptRequest{
						PxlScript: "import px",
					},
				},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_GenerateOTelScriptResp{
						GenerateOTelScriptResp: &vizierpb.GenerateOTelScriptResponse{
							Status: &vizierpb.Status{
								Code: int32(codes.OK),
							},
							OTelScript: "import px",
						},
					},
				},
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &vizierpb.Status{
							Code: int32(codes.OK),
						},
					},
				},
			},
		},
		{
			name:      "otel script: status error",
			requestID: "1",
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &cvmsgspb.C2VAPIStreamRequest_GenerateOTelScriptReq{
					GenerateOTelScriptReq: &vizierpb.GenerateOTelScriptRequest{
						PxlScript: "status_error",
					},
				},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_GenerateOTelScriptResp{
						GenerateOTelScriptResp: &vizierpb.GenerateOTelScriptResponse{
							Status: &vizierpb.Status{
								Code:    int32(codes.InvalidArgument),
								Message: "invalid argument",
							},
						},
					},
				},
			},
		},
		{
			name:      "otel script: grpc error",
			requestID: "1",
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &cvmsgspb.C2VAPIStreamRequest_GenerateOTelScriptReq{
					GenerateOTelScriptReq: &vizierpb.GenerateOTelScriptRequest{
						PxlScript: "error",
					},
				},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &vizierpb.Status{
							Code:    int32(codes.Unknown),
							Message: "rpc error: code = Unknown desc = Failed",
						},
					},
				},
			},
		},
		{
			name:      "unknown message type",
			requestID: "1",
			request: &cvmsgspb.C2VAPIStreamRequest{
				Msg: &unknownC2VMsg{},
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &vizierpb.Status{
							Code:    int32(codes.InvalidArgument),
							Message: "Unknown request type %!s(<nil>)",
						},
					},
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ts, cleanup := createTestState(t)
			defer cleanup(t)

			client := vizierpb.NewVizierServiceClient(ts.conn)

			s, err := ptproxy.NewPassThroughProxy(ts.nc, client)
			require.NoError(t, err)
			go func() {
				err := s.Run()
				require.NoError(t, err)
			}()

			// Subscribe to reply channel.
			replyCh := make(chan *nats.Msg, 10)
			replySub, err := ts.nc.ChanSubscribe("v2c.reply-"+test.requestID, replyCh)
			require.NoError(t, err)
			defer func() {
				err := replySub.Unsubscribe()
				require.NoError(t, err)
			}()

			sr := test.request
			sr.RequestID = test.requestID
			sr.Token = "abcd"

			// Publish execute script request.

			reqAnyMsg, err := types.MarshalAny(sr)
			require.NoError(t, err)
			c2vMsg := &cvmsgspb.C2VMessage{
				Msg: reqAnyMsg,
			}
			b, err := c2vMsg.Marshal()
			require.NoError(t, err)

			err = ts.nc.Publish("c2v.VizierPassthroughRequest", b)
			require.NoError(t, err)

			numResp := 0
			for numResp < len(test.expectedResps) {
				select {
				case msg := <-replyCh:
					v2cMsg := &cvmsgspb.V2CMessage{}
					err := proto.Unmarshal(msg.Data, v2cMsg)
					require.NoError(t, err)
					resp := &cvmsgspb.V2CAPIStreamResponse{}
					err = types.UnmarshalAny(v2cMsg.Msg, resp)
					require.NoError(t, err)
					assert.Equal(t, test.expectedResps[numResp], resp)
					numResp++
				case <-time.After(defaultTimeout):
					t.Fatal("Timed out")
				}
			}

			if !test.sendCancel {
				return
			}

			sr = &cvmsgspb.C2VAPIStreamRequest{
				RequestID: test.requestID,
				Token:     "abcd",
				Msg: &cvmsgspb.C2VAPIStreamRequest_CancelReq{
					CancelReq: &cvmsgspb.C2VAPIStreamCancel{},
				},
			}
			reqAnyMsg, err = types.MarshalAny(sr)
			require.NoError(t, err)
			c2vMsg = &cvmsgspb.C2VMessage{
				Msg: reqAnyMsg,
			}
			b, err = c2vMsg.Marshal()
			require.NoError(t, err)

			err = ts.nc.Publish("c2v.VizierPassthroughRequest", b)
			require.NoError(t, err)

			cancelResp := &cvmsgspb.V2CAPIStreamResponse{
				RequestID: test.requestID,
				Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
					Status: &vizierpb.Status{
						Code: int32(codes.Canceled),
					},
				},
			}

			select {
			case msg := <-replyCh:
				v2cMsg := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, v2cMsg)
				require.NoError(t, err)
				resp := &cvmsgspb.V2CAPIStreamResponse{}
				err = types.UnmarshalAny(v2cMsg.Msg, resp)
				require.NoError(t, err)
				assert.Equal(t, cancelResp, resp)
			case <-time.After(defaultTimeout):
				t.Fatal("Timed out")
			}
		})
	}
}
