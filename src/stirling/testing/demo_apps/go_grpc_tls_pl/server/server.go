package main

import (
	"context"
	"net/http"

	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/server"
	"pixielabs.ai/pixielabs/src/stirling/testing/demo_apps/go_grpc_tls_pl/server/greetpb"
)

// Server is used to implement the Greeter.
type Server struct {
}

// SayHello responds to a the basic HelloRequest.
func (s *Server) SayHello(ctx context.Context, in *greetpb.HelloRequest) (*greetpb.HelloReply, error) {
	return &greetpb.HelloReply{Message: "Hello " + in.Name}, nil
}

func main() {
	log.WithField("service", "server").Info("Starting service")

	services.SetupService("server", 50400)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	e := env.New("withpixie.ai")
	s := server.NewPLServer(e, mux)
	greetpb.RegisterGreeterServer(s.GRPCServer(), &Server{})

	s.Start()
	s.StopOnInterrupt()
}
