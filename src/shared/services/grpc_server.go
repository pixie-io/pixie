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
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

var logrusEntry *log.Entry

func init() {
	logrusEntry = log.NewEntry(log.StandardLogger())
	grpc_logrus.ReplaceGrpcLogger(logrusEntry)
}

// GRPCServerOptions are configuration options that are passed to the GRPC server.
type GRPCServerOptions struct {
	DisableAuth    map[string]bool
	AuthMiddleware func(context.Context, env.Env) (string, error) // Currently only used by cloud api-server.
	GRPCServerOpts []grpc.ServerOption
}

func grpcUnaryInjectSession() grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		sCtx := authcontext.New()
		sCtx.Path = info.FullMethod
		return handler(authcontext.NewContext(ctx, sCtx), req)
	}
}

func grpcStreamInjectSession() grpc.StreamServerInterceptor {
	return func(srv interface{}, stream grpc.ServerStream, info *grpc.StreamServerInfo, handler grpc.StreamHandler) error {
		sCtx := authcontext.New()
		sCtx.Path = info.FullMethod
		wrapped := grpc_middleware.WrapServerStream(stream)
		wrapped.WrappedContext = authcontext.NewContext(stream.Context(), sCtx)
		return handler(srv, wrapped)
	}
}

func createGRPCAuthFunc(env env.Env, opts *GRPCServerOptions) func(context.Context) (context.Context, error) {
	return func(ctx context.Context) (context.Context, error) {
		var err error
		var token string

		sCtx, err := authcontext.FromContext(ctx)
		if err != nil {
			return nil, status.Errorf(codes.Internal, "missing session context: %v", err)
		}

		if _, ok := opts.DisableAuth[sCtx.Path]; ok {
			return ctx, nil
		}

		if opts.AuthMiddleware != nil {
			token, err = opts.AuthMiddleware(ctx, env)
			if err != nil {
				return nil, status.Errorf(codes.Internal, "Auth middleware failed: %v", err)
			}
		} else {
			token, err = grpc_auth.AuthFromMD(ctx, "bearer")
			if err != nil {
				return nil, err
			}
		}

		err = sCtx.UseJWTAuth(env.JWTSigningKey(), token)
		if err != nil {
			return nil, status.Errorf(codes.Unauthenticated, "invalid auth token: %v", err)
		}
		return ctx, nil
	}
}

// CreateGRPCServer creates a GRPC server with default middleware for our services.
func CreateGRPCServer(env env.Env, serverOpts *GRPCServerOptions) *grpc.Server {
	logrusOpts := []grpc_logrus.Option{
		grpc_logrus.WithDurationField(func(duration time.Duration) (key string, value interface{}) {
			return "time", duration
		}),
	}
	opts := []grpc.ServerOption{
		grpc_middleware.WithUnaryServerChain(
			grpc_ctxtags.UnaryServerInterceptor(),
			grpcUnaryInjectSession(),
			grpc_logrus.UnaryServerInterceptor(logrusEntry, logrusOpts...),
			grpc_auth.UnaryServerInterceptor(createGRPCAuthFunc(env, serverOpts)),
		),
		grpc_middleware.WithStreamServerChain(
			grpc_ctxtags.StreamServerInterceptor(),
			grpcStreamInjectSession(),
			grpc_logrus.StreamServerInterceptor(logrusEntry, logrusOpts...),
			grpc_auth.StreamServerInterceptor(createGRPCAuthFunc(env, serverOpts)),
		),
	}

	opts = append(opts, serverOpts.GRPCServerOpts...)

	grpcServer := grpc.NewServer(opts...)
	return grpcServer
}
