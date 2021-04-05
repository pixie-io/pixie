package ptproxy_test

import (
	"context"
	"fmt"
	"io"
	"net"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/grpc/test/bufconn"

	proto1 "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	public_vizierpb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
	"pixielabs.ai/pixielabs/src/cloud/api/ptproxy"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	env2 "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/server"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

const (
	bufSize        = 1024 * 1024
	defaultTimeout = 30 * time.Second
)

type testState struct {
	t   *testing.T
	lis *bufconn.Listener

	nc   *nats.Conn
	conn *grpc.ClientConn
}

func createTestState(t *testing.T) (*testState, func(t *testing.T)) {
	lis := bufconn.Listen(bufSize)
	env := env2.New()
	s := server.CreateGRPCServer(env, &server.GRPCServerOptions{})

	nc, natsCleanup := testingutils.MustStartTestNATS(t)

	public_vizierpb.RegisterVizierServiceServer(s, ptproxy.NewVizierPassThroughProxy(nc, &fakeVzMgr{}))
	pl_api_vizierpb.RegisterVizierDebugServiceServer(s, ptproxy.NewVizierPassThroughProxy(nc, &fakeVzMgr{}))

	eg := errgroup.Group{}
	eg.Go(func() error { return s.Serve(lis) })

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithInsecure())
	if err != nil {
		t.Fatalf("Failed to dial bufnet: %v", err)
	}

	cleanupFunc := func(t *testing.T) {
		natsCleanup()
		conn.Close()
		s.GracefulStop()

		err := eg.Wait()
		if err != nil {
			t.Fatalf("failed to start server: %v", err)
		}
	}

	return &testState{
		t:    t,
		lis:  nil,
		nc:   nc,
		conn: conn,
	}, cleanupFunc
}

func createDialer(lis *bufconn.Listener) func(ctx context.Context, url string) (net.Conn, error) {
	return func(ctx context.Context, url string) (conn net.Conn, e error) {
		return lis.Dial()
	}
}

func TestVizierPassThroughProxy_ExecuteScript(t *testing.T) {
	viper.Set("jwt_signing_key", "the-key")

	ts, cleanup := createTestState(t)
	defer cleanup(t)

	client := public_vizierpb.NewVizierServiceClient(ts.conn)
	validTestToken := testingutils.GenerateTestJWTToken(t, viper.GetString("jwt_signing_key"))

	testCases := []struct {
		name string

		clusterID      string
		authToken      string
		respFromVizier []*cvmsgspb.V2CAPIStreamResponse

		expGRPCError     error
		expGRPCResponses []*public_vizierpb.ExecuteScriptResponse
	}{
		{
			name: "Missing auth token",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: "",

			expGRPCError: status.Error(codes.Unauthenticated, "no auth"),
		},
		{
			name: "Bad auth token",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: "bad-token",

			expGRPCError: status.Error(codes.Unauthenticated, "no auth"),
		},
		{
			name: "Missing auth token",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: "",

			expGRPCError: status.Error(codes.Unauthenticated, "incorrect auth"),
		},
		{
			name: "Bad cluster ID",

			clusterID: "1111-2222-2222-333333333333",
			authToken: validTestToken,

			expGRPCError: status.Error(codes.InvalidArgument, "clusterID"),
		},
		{
			name: "Disconnected cluster",

			clusterID: "10000000-1111-2222-2222-333333333333",
			authToken: validTestToken,

			expGRPCError: ptproxy.ErrNotAvailable,
		},
		{
			name: "Unhealthy cluster",

			clusterID: "10000000-1111-2222-2222-333333333333",
			authToken: validTestToken,

			expGRPCError: ptproxy.ErrNotAvailable,
		},
		{
			name: "Normal Stream",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: validTestToken,
			respFromVizier: []*cvmsgspb.V2CAPIStreamResponse{
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{ExecResp: &public_vizierpb.ExecuteScriptResponse{QueryID: "abc"}},
				},
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{ExecResp: &public_vizierpb.ExecuteScriptResponse{QueryID: "abc"}},
				},
			},

			expGRPCError: nil,
			expGRPCResponses: []*public_vizierpb.ExecuteScriptResponse{
				{
					QueryID: "abc",
				},
				{
					QueryID: "abc",
				},
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctx := context.Background()
			if len(tc.authToken) > 0 {
				ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
					fmt.Sprintf("bearer %s", tc.authToken))
			}

			ctx, cancel := context.WithCancel(ctx)
			defer cancel()
			resp, err := client.ExecuteScript(ctx,
				&public_vizierpb.ExecuteScriptRequest{ClusterID: tc.clusterID})
			require.NoError(t, err)

			fv := newFakeVizier(t, uuid.FromStringOrNil(tc.clusterID), ts.nc)
			fv.Run(t, tc.respFromVizier)
			defer fv.Stop()

			grpcDataCh := make(chan *public_vizierpb.ExecuteScriptResponse)
			var gotReadErr error
			var eg errgroup.Group
			eg.Go(func() error {
				defer close(grpcDataCh)
				for {
					d, err := resp.Recv()
					if err != nil && err != io.EOF {
						gotReadErr = err
					}
					if err == io.EOF {
						return nil
					}
					if d == nil {
						return nil
					}
					grpcDataCh <- d
				}
			})

			var responses []*public_vizierpb.ExecuteScriptResponse
			eg.Go(func() error {
				timeout := time.NewTimer(defaultTimeout)
				defer timeout.Stop()

				for {
					select {
					case <-resp.Context().Done():
						return nil
					case <-timeout.C:
						return fmt.Errorf("timeout waiting for data on grpc channel")
					case msg := <-grpcDataCh:

						if msg == nil {
							return nil
						}
						responses = append(responses, msg)
					}
				}
			})

			err = eg.Wait()
			if err != nil {
				t.Fatalf("Got error while streaming grpc: %v", err)
			}

			if tc.expGRPCError != nil {
				if gotReadErr == nil {
					t.Fatal("Expected to get GRPC error")
				}
				assert.Equal(t, status.Code(tc.expGRPCError), status.Code(gotReadErr))
			}
			if tc.expGRPCResponses == nil {
				if len(responses) != 0 {
					t.Fatal("Expected to get no responses")
				}
			} else {
				assert.Equal(t, tc.expGRPCResponses, responses)
			}
		})
	}
}

func TestVizierPassThroughProxy_HealthCheck(t *testing.T) {
	viper.Set("jwt_signing_key", "the-key")

	ts, cleanup := createTestState(t)
	defer cleanup(t)

	client := public_vizierpb.NewVizierServiceClient(ts.conn)
	validTestToken := testingutils.GenerateTestJWTToken(t, viper.GetString("jwt_signing_key"))

	testCases := []struct {
		name string

		clusterID      string
		authToken      string
		respFromVizier []*cvmsgspb.V2CAPIStreamResponse

		expGRPCError     error
		expGRPCResponses []*public_vizierpb.HealthCheckResponse
	}{
		{
			name: "Missing auth token",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: "",

			expGRPCError: status.Error(codes.Unauthenticated, "no auth"),
		},
		{
			name: "Bad auth token",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: "bad-token",

			expGRPCError: status.Error(codes.Unauthenticated, "no auth"),
		},
		{
			name: "Missing auth token",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: "",

			expGRPCError: status.Error(codes.Unauthenticated, "incorrect auth"),
		},
		{
			name: "Bad cluster ID",

			clusterID: "1111-2222-2222-333333333333",
			authToken: validTestToken,

			expGRPCError: status.Error(codes.InvalidArgument, "clusterID"),
		},
		{
			name: "Disconnected cluster",

			clusterID: "10000000-1111-2222-2222-333333333333",
			authToken: validTestToken,

			expGRPCError: ptproxy.ErrNotAvailable,
		},
		{
			name: "Unhealthy cluster",

			clusterID: "10000000-1111-2222-2222-333333333333",
			authToken: validTestToken,

			expGRPCError: ptproxy.ErrNotAvailable,
		},
		{
			name: "Normal Stream",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: validTestToken,
			respFromVizier: []*cvmsgspb.V2CAPIStreamResponse{
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_HcResp{HcResp: &public_vizierpb.HealthCheckResponse{Status: &public_vizierpb.Status{Code: 0}}},
				},
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_HcResp{HcResp: &public_vizierpb.HealthCheckResponse{Status: &public_vizierpb.Status{Code: 1}}},
				},
			},

			expGRPCError: nil,
			expGRPCResponses: []*public_vizierpb.HealthCheckResponse{
				{Status: &public_vizierpb.Status{Code: 0}},
				{Status: &public_vizierpb.Status{Code: 1}},
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctx := context.Background()
			if len(tc.authToken) > 0 {
				ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
					fmt.Sprintf("bearer %s", tc.authToken))
			}

			ctx, cancel := context.WithCancel(ctx)
			defer cancel()
			resp, err := client.HealthCheck(ctx,
				&public_vizierpb.HealthCheckRequest{ClusterID: tc.clusterID})
			require.NoError(t, err)

			fv := newFakeVizier(t, uuid.FromStringOrNil(tc.clusterID), ts.nc)
			fv.Run(t, tc.respFromVizier)
			defer fv.Stop()

			grpcDataCh := make(chan *public_vizierpb.HealthCheckResponse)
			var gotReadErr error

			var eg errgroup.Group
			eg.Go(func() error {
				defer close(grpcDataCh)
				for {
					d, err := resp.Recv()
					if err != nil && err != io.EOF {
						gotReadErr = err
					}
					if err == io.EOF {
						return nil
					}
					if d == nil {
						return nil
					}
					grpcDataCh <- d
				}
			})

			var responses []*public_vizierpb.HealthCheckResponse
			eg.Go(func() error {
				timeout := time.NewTimer(defaultTimeout)
				defer timeout.Stop()
				for {
					select {
					case <-resp.Context().Done():
						return nil
					case <-timeout.C:
						return fmt.Errorf("timeout waiting for grpc data")
					case msg := <-grpcDataCh:
						if msg == nil {
							return nil
						}
						responses = append(responses, msg)
					}
				}
			})

			err = eg.Wait()
			if err != nil {
				t.Fatal(err)
			}

			if tc.expGRPCError != nil {
				if gotReadErr == nil {
					t.Fatal("Expected to get GRPC error")
				}
				assert.Equal(t, status.Code(tc.expGRPCError), status.Code(gotReadErr))
			}
			if tc.expGRPCResponses == nil {
				if len(responses) != 0 {
					t.Fatal("Expected to get no responses")
				}
			} else {
				assert.Equal(t, tc.expGRPCResponses, responses)
			}
		})
	}
}

func TestVizierPassThroughProxy_DebugLog(t *testing.T) {
	viper.Set("jwt_signing_key", "the-key")

	ts, cleanup := createTestState(t)
	defer cleanup(t)

	client := pl_api_vizierpb.NewVizierDebugServiceClient(ts.conn)
	validTestToken := testingutils.GenerateTestJWTToken(t, viper.GetString("jwt_signing_key"))

	testCases := []struct {
		name string

		clusterID      string
		authToken      string
		respFromVizier []*cvmsgspb.V2CAPIStreamResponse

		expGRPCError     error
		expGRPCResponses []*pl_api_vizierpb.DebugLogResponse
	}{
		{
			name: "Normal Stream",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: validTestToken,
			respFromVizier: []*cvmsgspb.V2CAPIStreamResponse{
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_DebugLogResp{DebugLogResp: &pl_api_vizierpb.DebugLogResponse{Data: "test log 1"}},
				},
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_DebugLogResp{DebugLogResp: &pl_api_vizierpb.DebugLogResponse{Data: "test log 2"}},
				},
			},

			expGRPCError: nil,
			expGRPCResponses: []*pl_api_vizierpb.DebugLogResponse{
				{Data: "test log 1"},
				{Data: "test log 2"},
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctx := context.Background()
			if len(tc.authToken) > 0 {
				ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
					fmt.Sprintf("bearer %s", tc.authToken))
			}

			ctx, cancel := context.WithCancel(ctx)
			defer cancel()
			resp, err := client.DebugLog(ctx,
				&pl_api_vizierpb.DebugLogRequest{ClusterID: tc.clusterID})
			require.NoError(t, err)

			fv := newFakeVizier(t, uuid.FromStringOrNil(tc.clusterID), ts.nc)
			fv.Run(t, tc.respFromVizier)
			defer fv.Stop()

			grpcDataCh := make(chan *pl_api_vizierpb.DebugLogResponse)
			var gotReadErr error
			var eg errgroup.Group
			eg.Go(func() error {
				defer close(grpcDataCh)
				for {
					d, err := resp.Recv()
					if err != nil && err != io.EOF {
						gotReadErr = err
					}
					if err == io.EOF {
						return nil
					}
					if d == nil {
						return nil
					}
					grpcDataCh <- d
				}
			})

			responses := make([]*pl_api_vizierpb.DebugLogResponse, 0)
			eg.Go(func() error {
				timeout := time.NewTimer(defaultTimeout)
				defer timeout.Stop()
				for {
					select {
					case <-resp.Context().Done():
						return nil
					case <-timeout.C:
						return fmt.Errorf("timeout")
					case msg := <-grpcDataCh:
						if msg == nil {
							return nil
						}
						responses = append(responses, msg)
					}
				}
			})
			eg.Wait()
			if err != nil {
				t.Fatal(err)
			}

			if tc.expGRPCError != nil {
				if gotReadErr == nil {
					t.Fatal("Expected to get GRPC error")
				}
				assert.Equal(t, status.Code(tc.expGRPCError), status.Code(gotReadErr))
			}
			if tc.expGRPCResponses == nil {
				if len(responses) != 0 {
					t.Fatal("Expected to get no responses")
				}
			} else {
				assert.Equal(t, tc.expGRPCResponses, responses)
			}
		})
	}
}

func TestVizierPassThroughProxy_DebugPods(t *testing.T) {
	viper.Set("jwt_signing_key", "the-key")

	ts, cleanup := createTestState(t)
	defer cleanup(t)

	client := pl_api_vizierpb.NewVizierDebugServiceClient(ts.conn)
	validTestToken := testingutils.GenerateTestJWTToken(t, viper.GetString("jwt_signing_key"))

	testCases := []struct {
		name string

		clusterID      string
		authToken      string
		respFromVizier []*cvmsgspb.V2CAPIStreamResponse

		expGRPCError     error
		expGRPCResponses []*pl_api_vizierpb.DebugPodsResponse
	}{
		{
			name: "Normal Stream",

			clusterID: "00000000-1111-2222-2222-333333333333",
			authToken: validTestToken,
			respFromVizier: []*cvmsgspb.V2CAPIStreamResponse{
				{
					Msg: &cvmsgspb.V2CAPIStreamResponse_DebugPodsResp{
						DebugPodsResp: &pl_api_vizierpb.DebugPodsResponse{
							ControlPlanePods: []*pl_api_vizierpb.VizierPodStatus{
								&pl_api_vizierpb.VizierPodStatus{
									Name: "one pod",
								},
							},
							DataPlanePods: []*pl_api_vizierpb.VizierPodStatus{
								&pl_api_vizierpb.VizierPodStatus{
									Name: "another pod",
								},
							},
						},
					},
				},
			},

			expGRPCError: nil,
			expGRPCResponses: []*pl_api_vizierpb.DebugPodsResponse{
				&pl_api_vizierpb.DebugPodsResponse{
					ControlPlanePods: []*pl_api_vizierpb.VizierPodStatus{
						&pl_api_vizierpb.VizierPodStatus{
							Name: "one pod",
						},
					},
					DataPlanePods: []*pl_api_vizierpb.VizierPodStatus{
						&pl_api_vizierpb.VizierPodStatus{
							Name: "another pod",
						},
					},
				},
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ctx := context.Background()
			if len(tc.authToken) > 0 {
				ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
					fmt.Sprintf("bearer %s", tc.authToken))
			}

			ctx, cancel := context.WithCancel(ctx)
			defer cancel()
			resp, err := client.DebugPods(ctx,
				&pl_api_vizierpb.DebugPodsRequest{ClusterID: tc.clusterID})
			assert.Nil(t, err)

			fv := newFakeVizier(t, uuid.FromStringOrNil(tc.clusterID), ts.nc)
			fv.Run(t, tc.respFromVizier)
			defer fv.Stop()

			grpcDataCh := make(chan *pl_api_vizierpb.DebugPodsResponse)
			var gotReadErr error
			var eg errgroup.Group
			eg.Go(func() error {
				defer close(grpcDataCh)
				for {
					d, err := resp.Recv()
					if err != nil && err != io.EOF {
						gotReadErr = err
					}
					if err == io.EOF {
						return nil
					}
					if d == nil {
						return nil
					}
					grpcDataCh <- d
				}
			})

			var responses []*pl_api_vizierpb.DebugPodsResponse
			eg.Go(func() error {
				timeout := time.NewTimer(defaultTimeout)
				defer timeout.Stop()
				for {
					select {
					case <-resp.Context().Done():
						return nil
					case <-timeout.C:
						return fmt.Errorf("timeout")
					case msg := <-grpcDataCh:
						if msg == nil {
							return nil
						}
						responses = append(responses, msg)
					}
				}
			})

			err = eg.Wait()
			if err != nil {
				t.Fatal(err)
			}

			if tc.expGRPCError != nil {
				if gotReadErr == nil {
					t.Fatal("Expected to get GRPC error")
				}
				assert.Equal(t, status.Code(tc.expGRPCError), status.Code(gotReadErr))
			}
			if tc.expGRPCResponses == nil {
				if len(responses) != 0 {
					t.Fatal("Expected to get no responses")
				}
			} else {
				assert.Equal(t, tc.expGRPCResponses, responses)
			}
		})
	}
}

type fakeVzMgr struct{}

func (v *fakeVzMgr) GetVizierInfo(ctx context.Context, in *proto1.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierInfo, error) {
	bakedResponses := map[string]struct {
		info *cvmsgspb.VizierInfo
		err  error
	}{
		"00000000-1111-2222-2222-333333333333": {
			&cvmsgspb.VizierInfo{
				VizierID:        utils.ProtoFromUUIDStrOrNil("00000000-1111-2222-2222-333333333333"),
				Status:          cvmsgspb.VZ_ST_HEALTHY,
				LastHeartbeatNs: 0,
			},
			nil,
		},
		"10000000-1111-2222-2222-333333333333": {
			&cvmsgspb.VizierInfo{
				VizierID:        utils.ProtoFromUUIDStrOrNil("10000000-1111-2222-2222-333333333333"),
				Status:          cvmsgspb.VZ_ST_DISCONNECTED,
				LastHeartbeatNs: 0,
			},
			nil,
		},
		"20000000-1111-2222-2222-333333333333": {
			&cvmsgspb.VizierInfo{
				VizierID:        utils.ProtoFromUUIDStrOrNil("10000000-1111-2222-2222-333333333333"),
				Status:          cvmsgspb.VZ_ST_UNHEALTHY,
				LastHeartbeatNs: 0,
			},
			nil,
		},
	}

	u := utils.UUIDFromProtoOrNil(in)
	results, ok := bakedResponses[u.String()]
	if !ok {
		return nil, status.Error(codes.NotFound, "Cluster not found")
	}
	return results.info, results.err
}

func (v *fakeVzMgr) GetVizierConnectionInfo(ctx context.Context, in *proto1.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierConnectionInfo, error) {
	bakedResponses := map[string]struct {
		info *cvmsgspb.VizierConnectionInfo
		err  error
	}{
		"00000000-1111-2222-2222-333333333333": {
			&cvmsgspb.VizierConnectionInfo{
				IPAddress: "1.1.1.1",
				Token:     "abc0",
			},
			nil,
		},
		"10000000-1111-2222-2222-333333333333": {
			&cvmsgspb.VizierConnectionInfo{
				IPAddress: "2.2.2.2",
				Token:     "abc1",
			},
			nil,
		},
		"20000000-1111-2222-2222-333333333333": {
			&cvmsgspb.VizierConnectionInfo{
				IPAddress: "3.3.3.3",
				Token:     "abc2",
			},
			nil,
		},
	}
	u := utils.UUIDFromProtoOrNil(in)
	results, ok := bakedResponses[u.String()]
	if !ok {
		return nil, status.Error(codes.NotFound, "Cluster not found")
	}
	return results.info, results.err
}

type fakeVizier struct {
	t    *testing.T
	id   uuid.UUID
	nc   *nats.Conn
	ns   *nats.Subscription
	wg   sync.WaitGroup
	done chan bool
}

func newFakeVizier(t *testing.T, id uuid.UUID, nc *nats.Conn) *fakeVizier {
	return &fakeVizier{
		t:    t,
		id:   id,
		nc:   nc,
		wg:   sync.WaitGroup{},
		done: make(chan bool),
	}
}

func (f *fakeVizier) Run(t *testing.T, responses []*cvmsgspb.V2CAPIStreamResponse) {
	ch := make(chan *nats.Msg)
	topic := vzshard.C2VTopic("VizierPassthroughRequest", f.id)
	sub, err := f.nc.ChanSubscribe(topic, ch)
	if err != nil {
		t.Fatal(err)
	}
	f.ns = sub
	f.wg.Add(1)

	go func() {
		defer f.wg.Done()
		for {
			select {
			case <-f.done:
				return
			case msg := <-ch:
				c2vReq := cvmsgspb.C2VMessage{VizierID: f.id.String()}
				err := c2vReq.Unmarshal(msg.Data)
				require.NoError(t, err)
				req := &cvmsgspb.C2VAPIStreamRequest{}
				assert.Nil(t, types.UnmarshalAny(c2vReq.Msg, req))

				replyTopic := vzshard.V2CTopic(fmt.Sprintf("reply-%s", req.RequestID), f.id)
				for _, resp := range responses {
					resp.RequestID = req.RequestID
					err = f.nc.Publish(replyTopic, f.createV2CMessage(resp))
					require.NoError(t, err)
				}
				// Send completion message.
				fin := &cvmsgspb.V2CAPIStreamResponse{
					RequestID: req.RequestID,
					Msg: &cvmsgspb.V2CAPIStreamResponse_Status{Status: &public_vizierpb.Status{
						Code: 0,
					}},
				}
				err = f.nc.Publish(replyTopic, f.createV2CMessage(fin))
				require.NoError(t, err)
			}
		}
	}()
}

func (f *fakeVizier) createV2CMessage(response *cvmsgspb.V2CAPIStreamResponse) []byte {
	m := &cvmsgspb.V2CMessage{
		VizierID: f.id.String(),
	}
	anyPB, err := types.MarshalAny(response)
	assert.Nil(f.t, err)
	m.Msg = anyPB

	b, err := m.Marshal()
	assert.Nil(f.t, err)

	return b
}

func (f *fakeVizier) Stop() {
	close(f.done)
	err := f.ns.Unsubscribe()
	require.NoError(f.t, err)

	f.wg.Wait()
}
