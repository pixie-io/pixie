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

package server_test

import (
	"context"
	"fmt"
	"net"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/grpc/test/bufconn"

	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/server"
	ping "px.dev/pixie/src/shared/services/testproto"
	"px.dev/pixie/src/utils/testingutils"
)

const bufSize = 1024 * 1024

func init() {
	// Test will fail with SSL enabled since we don't expose certs to the tests.
	viper.Set("disable_ssl", true)
}

type testserver struct{}

func (s *testserver) Ping(ctx context.Context, in *ping.PingRequest) (*ping.PingReply, error) {
	return &ping.PingReply{Reply: "test reply"}, nil
}

func (s *testserver) PingServerStream(in *ping.PingRequest, srv ping.PingService_PingServerStreamServer) error {
	err := srv.Send(&ping.PingReply{Reply: "test reply"})
	if err != nil {
		return err
	}
	return nil
}

func (s *testserver) PingClientStream(srv ping.PingService_PingClientStreamServer) error {
	msg, err := srv.Recv()
	if err != nil {
		return err
	}
	if msg == nil {
		return fmt.Errorf("Got a nil message")
	}
	err = srv.SendAndClose(&ping.PingReply{Reply: "test reply"})
	if err != nil {
		return err
	}
	return nil
}

func startTestGRPCServer(opts *server.GRPCServerOptions) (*bufconn.Listener, func(t *testing.T)) {
	viper.Set("jwt_signing_key", "abc")
	var s *grpc.Server
	if opts == nil {
		opts = &server.GRPCServerOptions{}
	}

	s = server.CreateGRPCServer(env.New("withpixie.ai"), opts)

	ping.RegisterPingServiceServer(s, &testserver{})
	lis := bufconn.Listen(bufSize)

	eg := errgroup.Group{}
	eg.Go(func() error { return s.Serve(lis) })

	cleanupFunc := func(t *testing.T) {
		s.GracefulStop()

		err := eg.Wait()
		if err != nil {
			t.Fatalf("failed to start server: %v", err)
		}
	}
	return lis, cleanupFunc
}

func createDialer(lis *bufconn.Listener) func(ctx context.Context, url string) (net.Conn, error) {
	return func(ctx context.Context, url string) (net.Conn, error) {
		return lis.Dial()
	}
}

func makeTestRequest(ctx context.Context, t *testing.T, lis *bufconn.Listener) (*ping.PingReply, error) {
	grpc.WithContextDialer(createDialer(lis))
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := ping.NewPingServiceClient(conn)
	return c.Ping(ctx, &ping.PingRequest{Req: "hello"})
}

func makeTestClientStreamRequest(ctx context.Context, t *testing.T, lis *bufconn.Listener) (*ping.PingReply, error) {
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := ping.NewPingServiceClient(conn)

	stream, err := c.PingClientStream(ctx)
	if err != nil {
		t.Fatalf("Could not create stream")
	}

	err = stream.Send(&ping.PingRequest{Req: "hello"})
	if err != nil {
		t.Fatalf("Could not send on stream")
	}

	return stream.CloseAndRecv()
}

func makeTestServerStreamRequest(ctx context.Context, t *testing.T, lis *bufconn.Listener) (*ping.PingReply, error) {
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := ping.NewPingServiceClient(conn)

	stream, err := c.PingServerStream(ctx, &ping.PingRequest{Req: "hello"})
	if err != nil {
		t.Fatalf("Could not create stream")
	}

	return stream.Recv()
}

func TestGrpcServerUnary(t *testing.T) {
	tests := []struct {
		name         string
		token        string
		clientStream bool
		serverStream bool
		expectError  bool
		serverOpts   *server.GRPCServerOptions
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
			name:         "success - client stream",
			token:        "abc",
			expectError:  false,
			clientStream: true,
		},
		{
			name:         "bad token - client stream",
			token:        "bad.jwt.token",
			expectError:  true,
			clientStream: true,
		},
		{
			name:         "unauthenticated - client stream",
			token:        "",
			expectError:  true,
			clientStream: true,
		},
		{
			name:         "success - server stream",
			token:        "abc",
			expectError:  false,
			serverStream: true,
		},
		{
			name:         "bad token - server stream",
			token:        "bad.jwt.token",
			expectError:  true,
			serverStream: true,
		},
		{
			name:         "unauthenticated - server stream",
			token:        "",
			expectError:  true,
			serverStream: true,
		},
		{
			name:         "disable auth - unary",
			token:        "",
			expectError:  false,
			clientStream: false,
			serverOpts: &server.GRPCServerOptions{
				DisableAuth: map[string]bool{
					"/px.common.PingService/Ping": true,
				},
			},
		},
		{
			name:         "authmiddleware",
			token:        "",
			expectError:  false,
			clientStream: false,
			serverOpts: &server.GRPCServerOptions{
				AuthMiddleware: func(context.Context, env.Env) (string, error) {
					return testingutils.GenerateTestJWTToken(t, "abc"), nil
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			lis, cleanup := startTestGRPCServer(test.serverOpts)
			defer cleanup(t)

			var ctx context.Context
			if test.token != "" {
				token := testingutils.GenerateTestJWTToken(t, test.token)
				ctx = metadata.AppendToOutgoingContext(context.Background(),
					"authorization", "bearer "+token)
			} else {
				ctx = context.Background()
			}

			var resp *ping.PingReply
			var err error

			switch {
			case test.clientStream:
				resp, err = makeTestClientStreamRequest(ctx, t, lis)
			case test.serverStream:
				resp, err = makeTestServerStreamRequest(ctx, t, lis)
			default:
				resp, err = makeTestRequest(ctx, t, lis)
			}
			if test.expectError {
				assert.NotNil(t, err)
				stat, ok := status.FromError(err)
				assert.True(t, ok)
				assert.Equal(t, codes.Unauthenticated, stat.Code())
				assert.Nil(t, resp)
			} else {
				require.NoError(t, err)
				assert.NotNil(t, resp)
				assert.Equal(t, "test reply", resp.Reply)
			}
		})
	}
}
