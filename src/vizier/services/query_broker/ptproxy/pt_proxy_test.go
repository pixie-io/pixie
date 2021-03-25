package ptproxy_test

import (
	"context"
	"errors"
	"net"
	"testing"
	"time"

	"golang.org/x/sync/errgroup"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/test/bufconn"

	public_vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/ptproxy"
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

func (m *MockVzServer) ExecuteScript(req *public_vizierapipb.ExecuteScriptRequest, srv public_vizierapipb.VizierService_ExecuteScriptServer) error {
	if req.QueryStr == "should pass" {
		resp := &public_vizierapipb.ExecuteScriptResponse{
			QueryID: "1",
		}
		srv.Send(resp)
	}
	if req.QueryStr == "error" {
		resp := &public_vizierapipb.ExecuteScriptResponse{
			QueryID: "2",
		}
		srv.Send(resp)
		return errors.New("Failed")
	}
	if req.QueryStr == "cancel" {
		resp := &public_vizierapipb.ExecuteScriptResponse{
			QueryID: "3",
		}
		srv.Send(resp)
		<-time.After(defaultTimeout)
		return errors.New("Timeout waiting for context to be canceled")
	}
	return nil
}

func (m *MockVzServer) HealthCheck(req *public_vizierapipb.HealthCheckRequest, srv public_vizierapipb.VizierService_HealthCheckServer) error {
	return nil
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
	public_vizierapipb.RegisterVizierServiceServer(s, vzServer)

	eg := errgroup.Group{}
	eg.Go(func() error { return s.Serve(lis) })

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithInsecure())
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
	return func(ctx context.Context, url string) (conn net.Conn, e error) {
		return lis.Dial()
	}
}

func TestPassThroughProxy(t *testing.T) {
	tests := []struct {
		name          string
		requestID     string
		request       *public_vizierapipb.ExecuteScriptRequest
		expectedResps []*cvmsgspb.V2CAPIStreamResponse
		sendCancel    bool
	}{
		{
			name:      "complete",
			requestID: "1",
			request: &public_vizierapipb.ExecuteScriptRequest{
				QueryStr: "should pass",
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
						ExecResp: &public_vizierapipb.ExecuteScriptResponse{
							QueryID: "1",
						},
					},
				},
				{
					RequestID: "1",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &public_vizierapipb.Status{
							Code: int32(codes.OK),
						},
					},
				},
			},
		},
		{
			name:      "error",
			requestID: "2",
			request: &public_vizierapipb.ExecuteScriptRequest{
				QueryStr: "error",
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "2",
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
						ExecResp: &public_vizierapipb.ExecuteScriptResponse{
							QueryID: "2",
						},
					},
				},
				{
					RequestID: "2",
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
						Status: &public_vizierapipb.Status{
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
			request: &public_vizierapipb.ExecuteScriptRequest{
				QueryStr: "cancel",
			},
			expectedResps: []*cvmsgspb.V2CAPIStreamResponse{
				{
					RequestID: "3",
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
						ExecResp: &public_vizierapipb.ExecuteScriptResponse{
							QueryID: "3",
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

			client := public_vizierapipb.NewVizierServiceClient(ts.conn)

			s, err := ptproxy.NewPassThroughProxy(ts.nc, client)
			require.NoError(t, err)
			go s.Run()

			// Subscribe to reply channel.
			replyCh := make(chan *nats.Msg, 10)
			replySub, err := ts.nc.ChanSubscribe("v2c.reply-"+test.requestID, replyCh)
			require.NoError(t, err)
			defer replySub.Unsubscribe()

			// Publish execute script request.
			sr := &cvmsgspb.C2VAPIStreamRequest{
				RequestID: test.requestID,
				Token:     "abcd",
				Msg: &cvmsgspb.C2VAPIStreamRequest_ExecReq{
					ExecReq: test.request,
				},
			}
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
					Status: &public_vizierapipb.Status{
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
