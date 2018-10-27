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
	loginHandler, err := controllers.NewHandleLoginFunc()
	if err != nil {
		log.WithError(err).Panic("could not initialize login function")
	}
	mux.Handle("/auth/login", loginHandler)
	healthz.RegisterDefaultChecks(mux)

	env := &controllers.AuthEnv{
		Env: &common.Env{
			ExternalAddress: viper.GetString("external_addr"),
			SigningKey:      viper.GetString("jwt_signing_key"),
		},
	}
	s := common.NewPLServer(env, mux)
	auth.RegisterAuthServiceServer(s.GRPCServer(), &controllers.Server{})
	s.Start()
	s.StopOnInterrupt()
}
