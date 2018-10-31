package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/auth/authenv"
	"pixielabs.ai/pixielabs/services/auth/controllers"
	"pixielabs.ai/pixielabs/services/auth/proto"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
)

func main() {
	log.WithField("service", "auth-service").Info("Starting service")

	common.SetupService("auth-service", 50100)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	cfg := controllers.NewAuth0Config()
	a := controllers.NewAuth0Connector(cfg)
	if err := a.Init(); err != nil {
		log.WithError(err).Fatal("Failed to initialize Auth0")
	}

	env, err := authenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize auth gwenv")
	}

	server, err := controllers.NewServer(env, a)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")
	}

	s := common.NewPLServer(env, mux)
	auth.RegisterAuthServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
