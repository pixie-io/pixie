package common_test

import (
	"fmt"
	"net"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/services/common"
	commonenv "pixielabs.ai/pixielabs/services/common/env"
	pb "pixielabs.ai/pixielabs/services/common/testproto"
	"pixielabs.ai/pixielabs/utils/testingutils"
)

func init() {
	// Test will fail with SSL enabled since we don't expose certs to the tests.
	viper.Set("disable_ssl", true)
}

type server struct{}

func (s *server) Ping(ctx context.Context, in *pb.PingRequest) (*pb.PingReply, error) {
	return &pb.PingReply{Reply: "test reply"}, nil
}

func startTestGRPCServer(t *testing.T) (int, func()) {
	viper.Set("jwt_signing_key", "abc")
	env := commonenv.New()
	s := common.CreateGRPCServer(env)
	pb.RegisterPingServiceServer(s, &server{})
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

func makeTestRequest(ctx context.Context, t *testing.T, port int) (*pb.PingReply, error) {
	addr := fmt.Sprintf("localhost:%d", port)
	conn, err := grpc.Dial(addr, grpc.WithInsecure())
	if err != nil {
		t.Fatalf("did not connect: %v", err)
	}
	defer conn.Close()
	c := pb.NewPingServiceClient(conn)
	return c.Ping(ctx, &pb.PingRequest{Req: "hello"})
}

func TestCreateGRPCServer(t *testing.T) {
	port, cleanup := startTestGRPCServer(t)
	defer cleanup()

	token := testingutils.GenerateTestJWTToken(t, "abc")
	ctx := metadata.AppendToOutgoingContext(context.Background(),
		"Authorization", "bearer "+token)
	resp, err := makeTestRequest(ctx, t, port)

	assert.Nil(t, err)
	assert.Equal(t, "test reply", resp.Reply)
}

func TestCreateGRPCServer_BadToken(t *testing.T) {
	port, cleanup := startTestGRPCServer(t)
	defer cleanup()

	token := "bad.jwt.token"
	ctx := metadata.AppendToOutgoingContext(context.Background(),
		"Authorization", "bearer "+token)
	resp, err := makeTestRequest(ctx, t, port)

	assert.NotNil(t, err)
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.Unauthenticated, stat.Code())
	assert.Nil(t, resp)
}

func TestCreateGRPCServer_MissingAuth(t *testing.T) {
	port, cleanup := startTestGRPCServer(t)
	defer cleanup()

	resp, err := makeTestRequest(context.Background(), t, port)

	assert.NotNil(t, err)
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.Unauthenticated, stat.Code())
	assert.Nil(t, resp)
}
