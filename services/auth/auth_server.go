package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
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

	env := &controllers.AuthEnv{
		Env: &common.Env{
			ExternalAddress: viper.GetString("external_addr"),
			SigningKey:      viper.GetString("jwt_signing_key"),
		},
	}
	cfg := controllers.NewAuth0Config()

	a := controllers.NewAuth0Connector(cfg)
	if err := a.Init(); err != nil {
		log.WithError(err).Fatal("Failed to initialize Auth0")
	}
	server, err := controllers.NewServer(a)
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GRPC server funcs")

	}

	s := common.NewPLServer(env, mux)
	auth.RegisterAuthServiceServer(s.GRPCServer(), server)
	s.Start()
	s.StopOnInterrupt()
}
