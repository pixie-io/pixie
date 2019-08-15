package services

import (
	"time"

	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/env"

	grpc_middleware "github.com/grpc-ecosystem/go-grpc-middleware"
	grpc_auth "github.com/grpc-ecosystem/go-grpc-middleware/auth"
	grpc_logrus "github.com/grpc-ecosystem/go-grpc-middleware/logging/logrus"
	grpc_ctxtags "github.com/grpc-ecosystem/go-grpc-middleware/tags"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/status"
)

func grpcInjectSession() grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		sCtx := authcontext.New()
		return handler(authcontext.NewContext(ctx, sCtx), req)
	}
}

func createGRPCAuthFunc(env env.Env) func(context.Context) (context.Context, error) {
	return func(ctx context.Context) (context.Context, error) {
		if viper.GetBool("disable_grpc_auth") {
			return ctx, nil
		}
		token, err := grpc_auth.AuthFromMD(ctx, "bearer")
		if err != nil {
			return nil, err
		}

		sCtx, err := authcontext.FromContext(ctx)
		if err != nil {
			return nil, status.Errorf(codes.Internal, "missing session context: %v", err)
		}
		err = sCtx.UseJWTAuth(env.JWTSigningKey(), token)
		if err != nil {
			return nil, status.Errorf(codes.Unauthenticated, "invalid auth token: %v", err)
		}
		return ctx, nil
	}
}

// CreateGRPCServer creates a GRPC server with default middleware for our services.
func CreateGRPCServer(env env.Env) *grpc.Server {
	var tlsOpts grpc.ServerOption
	if !viper.GetBool("disable_ssl") {
		tlsConfig, err := DefaultServerTLSConfig()
		if err != nil {
			log.WithError(err).Fatal("Failed to load default server TLS config")
		}
		creds := credentials.NewTLS(tlsConfig)
		tlsOpts = grpc.Creds(creds)
	}

	logrusEntry := log.NewEntry(log.StandardLogger())
	logrusOpts := []grpc_logrus.Option{
		grpc_logrus.WithDurationField(func(duration time.Duration) (key string, value interface{}) {
			return "time", duration
		}),
	}
	grpc_logrus.ReplaceGrpcLogger(logrusEntry)
	opts := []grpc.ServerOption{
		grpc_middleware.WithUnaryServerChain(
			grpc_ctxtags.UnaryServerInterceptor(),
			grpcInjectSession(),
			grpc_logrus.UnaryServerInterceptor(logrusEntry, logrusOpts...),
			grpc_auth.UnaryServerInterceptor(createGRPCAuthFunc(env)),
		)}
	if !viper.GetBool("disable_ssl") {
		opts = append(opts, tlsOpts)
	}
	grpcServer := grpc.NewServer(opts...)
	return grpcServer
}
