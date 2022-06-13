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
	"io"
	"log"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/encoding/gzip"

	pb "px.dev/pixie/src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto"
)

func getDialOpts(compression, https bool) []grpc.DialOption {
	dialOpts := make([]grpc.DialOption, 0)

	if compression {
		dialOpts = append(dialOpts, grpc.WithDefaultCallOptions(grpc.UseCompressor(gzip.Name)))
	}

	if https {
		tlsConfig := &tls.Config{InsecureSkipVerify: true}
		creds := credentials.NewTLS(tlsConfig)
		dialOpts = append(dialOpts, grpc.WithTransportCredentials(creds))
	} else {
		dialOpts = append(dialOpts, grpc.WithTransportCredentials(insecure.NewCredentials()))
	}

	return dialOpts
}

func mustCreateGrpcClientConn(address string, compression, https bool) *grpc.ClientConn {
	// Set up a connection to the server.
	var conn *grpc.ClientConn
	var err error
	conn, err = grpc.Dial(address, getDialOpts(compression, https)...)
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	return conn
}

func streamGreet(address string, compression, https bool, name string) {
	conn := mustCreateGrpcClientConn(address, compression, https)

	defer conn.Close()

	c := pb.NewStreamingGreeterClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	stream, err := c.SayHelloServerStreaming(ctx, &pb.HelloRequest{Name: name})
	if err != nil {
		log.Fatalf("Failed to make streaming RPC call SayHelloServerStreaming(), error: %v", err)
	}
	for {
		item, err := stream.Recv()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalf("SayHelloServerStreaming() failed, error: %v", err)
		}
		log.Println(item.Message)
	}
}

func clientStreamGreet(address string, compression, https bool, names []string) {
	conn := mustCreateGrpcClientConn(address, compression, https)

	defer conn.Close()

	c := pb.NewStreamingGreeterClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	stream, err := c.SayHelloClientStreaming(ctx)
	if err != nil {
		log.Fatalf("Failed to make streaming RPC call SayHelloServerStreaming(), error: %v", err)
	}
	for _, name := range names {
		if err := stream.Send(&pb.HelloRequest{Name: name}); err != nil {
			if err == io.EOF {
				break
			}
			log.Fatalf("Send(%v) failed, error: %v", name, err)
		}
	}
	reply, err := stream.CloseAndRecv()
	if err != nil {
		log.Fatalf("Failed to close client stream, error: %v", err)
	}
	log.Println(reply.Message)
}

func bidirStreamGreet(address string, compression, https bool, names []string) {
	conn := mustCreateGrpcClientConn(address, compression, https)

	defer conn.Close()

	c := pb.NewStreamingGreeterClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()

	stream, err := c.SayHelloBidirStreaming(ctx)
	if err != nil {
		log.Fatalf("Failed to make streaming RPC call SayHelloServerStreaming(), error: %v", err)
	}
	for _, name := range names {
		if err := stream.Send(&pb.HelloRequest{Name: name}); err != nil {
			if err == io.EOF {
				break
			}
			log.Fatalf("Send(%v) failed, error: %v", name, err)
		}
		reply, err := stream.Recv()
		if err == io.EOF {
			return
		}
		if err != nil {
			log.Fatalf("Failed to receive server stream, error: %v", err)
		}
		log.Println(reply.Message)
	}
	err = stream.CloseSend()
	if err != nil {
		log.Fatalf("Failed to close send")
	}
}

func connectAndGreet(address string, compression, https bool, name string) {
	// Set up a connection to the server.
	conn := mustCreateGrpcClientConn(address, compression, https)

	defer conn.Close()

	c := pb.NewGreeterClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	r, err := c.SayHello(ctx, &pb.HelloRequest{Name: name})
	if err != nil {
		log.Fatalf("could not greet: %v", err)
	} else {
		log.Printf("Greeting: %s", r.Message)
	}
}

func main() {
	address := flag.String("address", "localhost:50051", "Server end point.")
	once := flag.Bool("once", false, "If true, send one request and wait for response and exit.")
	name := flag.String("name", "world", "The name to greet.")
	https := flag.Bool("https", false, "If true, uses https.")
	clientStreaming := flag.Bool("client_streaming", false, "Whether or not to call client streaming RPC")
	serverStreaming := flag.Bool("server_streaming", false, "Whether or not to call server streaming RPC")
	bidirStreaming := flag.Bool("bidir_streaming", false, "Whether or not to call server streaming RPC")
	compression := flag.Bool("compression", false, "Wether or not to use gRPC compression.")
	count := flag.Int("count", 1, "The count of requests to make.")
	waitPeriodMills := flag.Int("wait_period_millis", 500, "The waiting period between making successive requests.")

	flag.Parse()

	var fn func()
	switch {
	case *clientStreaming:
		fn = func() { clientStreamGreet(*address, *compression, *https, []string{*name, *name, *name}) }
	case *serverStreaming:
		fn = func() { streamGreet(*address, *compression, *https, *name) }
	case *bidirStreaming:
		fn = func() { bidirStreamGreet(*address, *compression, *https, []string{*name, *name, *name}) }
	default:
		fn = func() { connectAndGreet(*address, *compression, *https, *name) }
	}

	if *once {
		fn()
		return
	}

	for i := 0; i < *count; i++ {
		fn()
		time.Sleep(time.Duration(*waitPeriodMills) * time.Millisecond)
	}
}
