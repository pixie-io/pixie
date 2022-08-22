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

package main

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"net"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"golang.org/x/net/http2"
	"golang.org/x/net/http2/h2c"
	"google.golang.org/grpc"

	"px.dev/pixie/src/stirling/testing/demo_apps/go_grpc_tls_pl/server/greetpb"
)

const (
	port = ":50400"
)

// Server is used to implement the Greeter.
type Server struct{}

// SayHello responds to a the basic HelloRequest.
func (s *Server) SayHello(ctx context.Context, in *greetpb.HelloRequest) (*greetpb.HelloReply, error) {
	return &greetpb.HelloReply{Message: "Hello " + in.Name}, nil
}

func main() {
	pflag.String("server_tls_cert", "", "Path to server.crt")
	pflag.String("server_tls_key", "", "Path to server.key")
	pflag.String("tls_ca_cert", "", "Path to ca.crt")
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)

	pair, err := tls.LoadX509KeyPair(viper.GetString("server_tls_cert"), viper.GetString("server_tls_key"))
	if err != nil {
		log.WithError(err).Fatal("failed to load keys")
	}

	certPool := x509.NewCertPool()
	ca, err := os.ReadFile(viper.GetString("tls_ca_cert"))
	if err != nil {
		log.WithError(err).Fatal("failed to read CA cert")
	}

	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		log.Fatal("failed to append CA cert")
	}

	config := &tls.Config{
		Certificates: []tls.Certificate{pair},
		NextProtos:   []string{"h2"},
		ClientCAs:    certPool,
	}

	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	greetpb.RegisterGreeterServer(grpcServer, &Server{})

	muxHandler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		grpcServer.ServeHTTP(w, r)
	})

	httpServer := &http.Server{
		Addr:           port,
		Handler:        h2c.NewHandler(muxHandler, &http2.Server{}),
		TLSConfig:      config,
		ReadTimeout:    1800 * time.Second,
		WriteTimeout:   1800 * time.Second,
		MaxHeaderBytes: 1 << 20,
	}

	lis = tls.NewListener(lis, httpServer.TLSConfig)
	go httpServer.Serve(lis) //nolint:errcheck // This returns an error on graceful shutdown too.

	// The test relies on the following to check for server readiness
	log.Print("Starting HTTP/2 server")

	ch := make(chan os.Signal, 1)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	<-ch

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	err = httpServer.Shutdown(ctx)
	if err != nil {
		log.WithError(err).Error("http2 server Shutdown() failed")
	}
}
