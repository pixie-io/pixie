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
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"strconv"
	"strings"

	"google.golang.org/grpc"
	_ "google.golang.org/grpc/encoding/gzip"
	"google.golang.org/grpc/reflection"

	pb "px.dev/pixie/src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto"
)

// server is used to implement helloworld.GreeterServer.
type server struct{}

// SayHello implements helloworld.GreeterServer
func (s *server) SayHello(ctx context.Context, in *pb.HelloRequest) (*pb.HelloReply, error) {
	return &pb.HelloReply{Message: "Hello " + in.Name}, nil
}

func (s *server) SayHelloAgain(ctx context.Context, in *pb.HelloRequest) (*pb.HelloReply, error) {
	return &pb.HelloReply{Message: "Hello " + in.Name}, nil
}

func (s *server) SayHelloClientStreaming(srv pb.StreamingGreeter_SayHelloClientStreamingServer) error {
	names := []string{}
	for {
		helloReq, err := srv.Recv()
		if err == io.EOF {
			return srv.SendAndClose(&pb.HelloReply{Message: "Hello " + strings.Join(names, ", ") + "!"})
		}
		if err != nil {
			return err
		}
		names = append(names, helloReq.Name)
	}
}

func (s *server) SayHelloServerStreaming(in *pb.HelloRequest, srv pb.StreamingGreeter_SayHelloServerStreamingServer) error {
	log.Printf("SayHelloServerStreaming, in: %v\n", in)
	// Send 3 responses each time. We do not care much about the exact number of responses, this is for executing the
	// server streaming mechanism and observe the underlying HTTP2 framing data.
	for i := 0; i < 3; i++ {
		err := srv.Send(&pb.HelloReply{Message: "Hello " + in.Name})
		if err != nil {
			return err
		}
	}
	return nil
}

func (s *server) SayHelloBidirStreaming(stream pb.StreamingGreeter_SayHelloBidirStreamingServer) error {
	for {
		helloReq, err := stream.Recv()
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return err
		}
		err = stream.Send(&pb.HelloReply{Message: "Hello " + helloReq.Name})
		if err != nil {
			return err
		}
	}
}

func main() {
	var port = flag.Int("port", 50051, "The port to listen.")
	var https = flag.Bool("https", false, "Whether or not to use https")
	var cert = flag.String("cert", "", "Path to the .crt file.")
	var key = flag.String("key", "", "Path to the .key file.")
	var streaming = flag.Bool("streaming", false, "Whether or not to call streaming RPC")

	const keyPairBase = "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/go_grpc_server"

	flag.Parse()

	portStr := ":" + strconv.Itoa(*port)

	var lis net.Listener
	var err error
	if *https {
		certFile := keyPairBase + "/https-server.crt"
		if len(*cert) > 0 {
			certFile = *cert
		}
		keyFile := keyPairBase + "/https-server.key"
		if len(*key) > 0 {
			keyFile = *key
		}
		var c tls.Certificate
		c, err = tls.LoadX509KeyPair(certFile, keyFile)
		if err != nil {
			log.Fatalf("failed to load certs: %v", err)
		}
		tlsConfig := &tls.Config{Certificates: []tls.Certificate{c}}

		log.Printf("Starting https server on port : %s cert: %s key: %s", portStr, certFile, keyFile)
		lis, err = tls.Listen("tcp", portStr, tlsConfig)
	} else {
		log.Printf("Starting http server on port : %s", portStr)
		lis, err = net.Listen("tcp", portStr)
	}

	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	fmt.Print(lis.Addr().(*net.TCPAddr).Port)

	s := grpc.NewServer()

	if *streaming {
		log.Printf("Launching streaming server")
		pb.RegisterStreamingGreeterServer(s, &server{})
	} else {
		log.Printf("Launching unary server")
		pb.RegisterGreeterServer(s, &server{})
	}
	// Register reflection service on gRPC server.
	reflection.Register(s)
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
