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
	"crypto/tls"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"sync"
	"syscall"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"golang.org/x/net/http2"
	"golang.org/x/net/http2/h2c"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
)

func isGRPCRequest(r *http.Request) bool {
	return r.ProtoMajor == 2 && strings.HasPrefix(r.Header.Get("Content-Type"), "application/grpc")
}

// PLServer is the services server component used across all Pixie Labs services.
// It starts both an HTTP and a GRPC server and handles middelware and env injection.
type PLServer struct {
	ch          chan bool
	wg          *sync.WaitGroup
	grpcServer  *grpc.Server
	httpHandler http.Handler
	httpServer  *http.Server
}

// NewPLServer creates a new PLServer.
func NewPLServer(env env.Env, httpHandler http.Handler, grpcServerOpts ...grpc.ServerOption) *PLServer {
	opts := &GRPCServerOptions{GRPCServerOpts: grpcServerOpts}
	return NewPLServerWithOptions(env, httpHandler, opts)
}

// NewPLServerWithOptions creates a new PLServer.
func NewPLServerWithOptions(env env.Env, httpHandler http.Handler, opts *GRPCServerOptions) *PLServer {
	s := &PLServer{
		ch:          make(chan bool),
		wg:          &sync.WaitGroup{},
		grpcServer:  CreateGRPCServer(env, opts),
		httpHandler: httpHandler,
	}
	return s
}

// GRPCServer returns a pointer to the underlying GRPC server.
func (s *PLServer) GRPCServer() *grpc.Server {
	return s.grpcServer
}

func (s *PLServer) serveHTTP2() {
	s.wg.Add(1)
	defer s.wg.Done()
	// Register GRPC reflection.
	reflection.Register(s.grpcServer)

	sslEnabled := !viper.GetBool("disable_ssl")
	var tlsConfig *tls.Config
	if sslEnabled {
		var err error
		tlsConfig, err = services.DefaultServerTLSConfig()
		if err != nil {
			log.WithError(err).Fatal("Failed to load default server TLS config")
		}
	}
	serverAddr := fmt.Sprintf(":%d", viper.GetInt("http2_port"))
	// If it's a GRPC request we use the GRPC handler, otherwise forward to the regular HTTP(/2) handler.
	muxHandler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if isGRPCRequest(r) {
			// TODO(vihang, PP-2613): Remove this path renaming once migration is done.
			if strings.HasPrefix(r.URL.Path, "/pl.") {
				r.URL.Path = fmt.Sprintf("/px.%s", r.URL.Path[4:])
			}
			s.grpcServer.ServeHTTP(w, r)
			return
		}
		s.httpHandler.ServeHTTP(w, r)
	})
	wrappedHandler := services.HTTPLoggingMiddleware(muxHandler)
	s.httpServer = &http.Server{
		Addr:           serverAddr,
		Handler:        h2c.NewHandler(wrappedHandler, &http2.Server{}),
		TLSConfig:      tlsConfig,
		ReadTimeout:    1800 * time.Second,
		WriteTimeout:   1800 * time.Second,
		MaxHeaderBytes: 1 << 20,
	}
	log.WithField("addr", serverAddr).Print("Starting HTTP/2 server")
	lis, err := net.Listen("tcp", serverAddr)
	if err != nil {
		log.WithError(err).Fatal("Failed to listen (grpc)")
	}
	if sslEnabled {
		lis = tls.NewListener(lis, s.httpServer.TLSConfig)
	}
	if err := s.httpServer.Serve(lis); err != nil {
		// Check for graceful termination.
		if err != http.ErrServerClosed {
			log.WithError(err).Fatal("Failed to run GRPC server")
		}
	}
	log.Info("HTTP/2 server stopped.")
}

// Start runs the services in go routines. It returns immediately.
// On error in starting services the program will terminate.
func (s *PLServer) Start() {
	go s.serveHTTP2()
}

// Stop will gracefully shutdown underlying GRPC and HTTP servers.
func (s *PLServer) Stop() {
	log.Info("Stopping servers.")
	if s.grpcServer != nil {
		go s.grpcServer.Stop()
	}
	if s.httpServer != nil {
		wait := make(chan bool)
		go func() {
			defer close(wait)
			ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
			defer cancel()

			if err := s.httpServer.Shutdown(ctx); err != nil {
				log.WithError(err).Error("Failed to do a graceful shutdown of HTTP server.")
			}
			log.Info("Shutdown HTTP server complete. ")
		}()
		<-wait
	}
	s.wg.Wait()
	log.Info("Waiting is complete")
}

// StopOnInterrupt gracefully shuts down when ctrl-c is pressed or termination signal is received.
func (s *PLServer) StopOnInterrupt() {
	ch := make(chan os.Signal, 1)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	<-ch
	s.Stop()
}
