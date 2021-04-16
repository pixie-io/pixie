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

	pb "px.dev/pixie/src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto"
)

func mustCreateGrpcClientConn(address string, https bool) *grpc.ClientConn {
	// Set up a connection to the server.
	var conn *grpc.ClientConn
	var err error
	if https {
		tlsConfig := &tls.Config{InsecureSkipVerify: true}
		creds := credentials.NewTLS(tlsConfig)
		conn, err = grpc.Dial(address, grpc.WithTransportCredentials(creds))
	} else {
		conn, err = grpc.Dial(address, grpc.WithInsecure())
	}
	if err != nil {
		log.Fatalf("did not connect: %v", err)
	}
	return conn
}

func streamGreet(address string, https bool, name string) {
	conn := mustCreateGrpcClientConn(address, https)

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

func clientStreamGreet(address string, https bool, names []string) {
	conn := mustCreateGrpcClientConn(address, https)

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

func bidirStreamGreet(address string, https bool, names []string) {
	conn := mustCreateGrpcClientConn(address, https)

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

func connectAndGreet(address string, https bool, name string) {
	// Set up a connection to the server.
	conn := mustCreateGrpcClientConn(address, https)

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
	count := flag.Int("count", 1, "The count of requests to make.")
	waitPeriodMills := flag.Int("wait_period_millis", 500, "The waiting period between making successive requests.")

	flag.Parse()

	var fn func()
	switch {
	case *clientStreaming:
		fn = func() { clientStreamGreet(*address, *https, []string{*name, *name, *name}) }
	case *serverStreaming:
		fn = func() { streamGreet(*address, *https, *name) }
	case *bidirStreaming:
		fn = func() { bidirStreamGreet(*address, *https, []string{*name, *name, *name}) }
	default:
		fn = func() { connectAndGreet(*address, *https, *name) }
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
