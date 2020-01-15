package main

import (
	"crypto/tls"
	"flag"
	"fmt"
	"log"
	"net"
	"strconv"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
	pb "pixielabs.ai/pixielabs/src/stirling/http2/testing/proto"
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
	var port = flag.Int("port", 50051, "The port to listen.")
	var https = flag.Bool("https", false, "Whether or not to use https")
	var cert = flag.String("cert", "", "Path to the .crt file.")
	var key = flag.String("key", "", "Path to the .key file.")
	const keyPairBase = "src/stirling/http2/testing/go_grpc_server"

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
		cert, err := tls.LoadX509KeyPair(certFile, keyFile)
		if err != nil {
			log.Fatalf("failed to load certs: %v", err)
		}
		tlsConfig := &tls.Config{Certificates: []tls.Certificate{cert}}

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
	pb.RegisterGreeterServer(s, &server{})
	// Register reflection service on gRPC server.
	reflection.Register(s)
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
