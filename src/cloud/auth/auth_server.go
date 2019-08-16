package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/cloud/auth/authenv"
	"pixielabs.ai/pixielabs/src/cloud/auth/controllers"
	auth "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	log.WithField("service", "auth-service").Info("Starting service")

	services.SetupService("auth-service", 50100)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	cfg := controllers.NewAuth0Config()
	a := controllers.NewAuth0Connector(cfg)
	if err := a.Init(); err != nil {
		log.WithError(err).Fatal("Failed to initialize Auth0")
	}

	env, err := authenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize auth env")
	}

	server, err := controllers.NewServer(env, a)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := services.NewPLServer(env, mux)
	auth.RegisterAuthServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
