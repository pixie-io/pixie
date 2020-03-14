package services_test

import (
	"fmt"
	"net"
	"testing"

	"pixielabs.ai/pixielabs/src/shared/services"

	env2 "pixielabs.ai/pixielabs/src/shared/services/env"
	ping "pixielabs.ai/pixielabs/src/shared/services/testproto"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func init() {
	// Test will fail with SSL enabled since we don't expose certs to the tests.
	viper.Set("disable_ssl", true)
}

type server struct{}

func (s *server) Ping(ctx context.Context, in *ping.PingRequest) (*ping.PingReply, error) {
	return &ping.PingReply{Reply: "test reply"}, nil
}

func (s *server) PingStream(in *ping.PingRequest, srv ping.PingService_PingStreamServer) error {
	srv.Send(&ping.PingReply{Reply: "test reply"})
	return nil
}

func startTestGRPCServer(t *testing.T, opts *services.GRPCServerOptions) (int, func()) {
	viper.Set("jwt_signing_key", "abc")
	env := env2.New()
	var s *grpc.Server
	if opts == nil {
		opts = &services.GRPCServerOptions{}
	}

	s = services.CreateGRPCServer(env, opts)

	ping.RegisterPingServiceServer(s, &server{})
	lis, err := net.Listen("tcp", ":0")

	if err != nil {
		t.Fatalf("Failed to start listner: %v", err.Error())
	}

	go func() {
		if err := s.Serve(lis); err != nil {
			t.Fatalf("failed to serve: %v", err)
		}
	}()
	cleanupFunc := func() {
		s.Stop()
	}
	return lis.Addr().(*net.TCPAddr).Port, cleanupFunc
}

func makeTestRequest(ctx context.Context, t *testing.T, port int) (*ping.PingReply, error) {
	addr := fmt.Sprintf("localhost:%d", port)
	conn, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		t.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := ping.NewPingServiceClient(conn)
	return c.Ping(ctx, &ping.PingRequest{Req: "hello"})
}

func makeTestStreamRequest(ctx context.Context, t *testing.T, port int) (*ping.PingReply, error) {
	addr := fmt.Sprintf("localhost:%d", port)
	conn, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		t.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := ping.NewPingServiceClient(conn)

	stream, err := c.PingStream(ctx, &ping.PingRequest{Req: "hello"})
	if err != nil {
		t.Fatalf("Could not create stream")
	}

	return stream.Recv()
}

func TestGrpcServerUnary(t *testing.T) {
	tests := []struct {
		name        string
		token       string
		stream      bool
		expectError bool
		serverOpts  *services.GRPCServerOptions
	}{
		{
			name:        "success - unary",
			token:       "abc",
			expectError: false,
		},
		{
			name:        "bad token - unary",
			token:       "bad.jwt.token",
			expectError: true,
		},
		{
			name:        "unauthenticated - unary",
			token:       "",
			expectError: true,
		},
		{
			name:        "success - stream",
			token:       "abc",
			expectError: false,
			stream:      true,
		},
		{
			name:        "bad token - stream",
			token:       "bad.jwt.token",
			expectError: true,
			stream:      true,
		},
		{
			name:        "unauthenticated - stream",
			token:       "",
			expectError: true,
			stream:      true,
		},
		{
			name:        "disable auth - unary",
			token:       "",
			expectError: false,
			stream:      false,
			serverOpts: &services.GRPCServerOptions{
				DisableAuth: map[string]bool{
					"/pl.common.PingService/Ping": true,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			port, cleanup := startTestGRPCServer(t, test.serverOpts)
			defer cleanup()

			var ctx context.Context
			if test.token != "" {
				token := testingutils.GenerateTestJWTToken(t, test.token)
				ctx = metadata.AppendToOutgoingContext(context.Background(),
					"Authorization", "bearer "+token)
			} else {
				ctx = context.Background()
			}

			var resp *ping.PingReply
			var err error

			if test.stream {
				resp, err = makeTestStreamRequest(ctx, t, port)
			} else {
				resp, err = makeTestRequest(ctx, t, port)
			}

			if test.expectError {
				assert.NotNil(t, err)
				stat, ok := status.FromError(err)
				assert.True(t, ok)
				assert.Equal(t, codes.Unauthenticated, stat.Code())
				assert.Nil(t, resp)
			} else {
				assert.Nil(t, err)
				assert.Equal(t, "test reply", resp.Reply)
			}
		})
	}
}
