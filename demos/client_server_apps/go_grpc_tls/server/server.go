package main

import (
	"context"
	"crypto/tls"
	"log"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	pb "pixielabs.ai/pixielabs/src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto"
)

const (
	port = ":50051"
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

func main() {
	cert, err := tls.LoadX509KeyPair("https-server.crt", "https-server.key")
	if err != nil {
		log.Fatalf("failed to load certs: %v", err)
	}
	tlsConfig := &tls.Config{Certificates: []tls.Certificate{cert}}

	log.Printf("Starting server on port : %s", port)
	lis, err := tls.Listen("tcp", port, tlsConfig)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	s := grpc.NewServer()
	pb.RegisterGreeterServer(s, &server{})
	reflection.Register(s)
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
