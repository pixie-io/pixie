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

package server

import (
	"context"
	"errors"
	"time"

	grpc_middleware "github.com/grpc-ecosystem/go-grpc-middleware"
	grpc_auth "github.com/grpc-ecosystem/go-grpc-middleware/auth"
	grpc_logrus "github.com/grpc-ecosystem/go-grpc-middleware/logging/logrus"
	grpc_ctxtags "github.com/grpc-ecosystem/go-grpc-middleware/tags"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"

	// Enables gzip encoding for GRPC.
	_ "google.golang.org/grpc/encoding/gzip"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/env"
)

var logrusEntry *log.Entry

func init() {
	logrusEntry = log.NewEntry(log.StandardLogger())
	grpc_logrus.ReplaceGrpcLogger(logrusEntry)
}

// GRPCServerOptions are configuration options that are passed to the GRPC server.
type GRPCServerOptions struct {
	DisableAuth       map[string]bool
	AuthMiddleware    func(context.Context, env.Env) (string, error) // Currently only used by cloud api-server.
	GRPCServerOpts    []grpc.ServerOption
	DisableMiddleware bool
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

		err = sCtx.UseJWTAuth(env.JWTSigningKey(), token, env.Audience())
		if err != nil {
			return nil, status.Errorf(codes.Unauthenticated, "invalid auth token: %v", err)
		}
		return ctx, nil
	}
}

func codeToLevel(code codes.Code) log.Level {
	if code == codes.Unavailable {
		return log.DebugLevel
	}

	return grpc_logrus.DefaultClientCodeToLevel(code)
}

func logDecider(fullMethodName string, err error) bool {
	return !errors.Is(err, context.Canceled)
}

// CreateGRPCServer creates a GRPC server with default middleware for our services.
func CreateGRPCServer(env env.Env, serverOpts *GRPCServerOptions) *grpc.Server {
	logrusOpts := []grpc_logrus.Option{
		grpc_logrus.WithDurationField(func(duration time.Duration) (string, interface{}) {
			return "time", duration
		}),
		grpc_logrus.WithLevels(codeToLevel),
		grpc_logrus.WithDecider(logDecider),
	}
	opts := []grpc.ServerOption{}
	if !serverOpts.DisableMiddleware {
		opts = append(opts,
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
		)
	}

	opts = append(opts, serverOpts.GRPCServerOpts...)

	grpcServer := grpc.NewServer(opts...)
	return grpcServer
}
