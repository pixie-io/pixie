package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
	"pixielabs.ai/pixielabs/services/gateway/controllers"
)

func main() {
	log.WithField("service", "gateway-service").Info("Starting service")

	common.SetupService("gateway-service", 50000)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", http.HandlerFunc(controllers.AuthLoginHandler))
	mux.Handle("/api/auth/logout", http.HandlerFunc(controllers.AuthLogoutHandler))

	healthz.RegisterDefaultChecks(mux)
	env, err := controllers.NewGatewayEnv()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize gateway error")
	}
	s := common.NewPLServer(env, mux)
	s.Start()
	s.StopOnInterrupt()
}
