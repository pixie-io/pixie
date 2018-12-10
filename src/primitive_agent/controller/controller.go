package main

import (
	"log"
	"net"
	"os"

	"google.golang.org/grpc"

	pb "pixielabs.ai/pixielabs/src/primitive_agent/controller/proto"
	"pixielabs.ai/pixielabs/src/primitive_agent/controller/server"
)

func main() {
	port := os.Getenv("CONTROLLER_PORT")
	lis, err := net.Listen("tcp", port)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	s := grpc.NewServer()

	// Set up output file.
	outputFile, _ := os.Create("/output.txt")
	defer outputFile.Close()

	pb.RegisterControllerServer(s, controllerserver.New(outputFile))
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}
