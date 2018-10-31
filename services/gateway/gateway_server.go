package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/handler"
	"pixielabs.ai/pixielabs/services/common/healthz"
	"pixielabs.ai/pixielabs/services/gateway/controllers"
	"pixielabs.ai/pixielabs/services/gateway/gwenv"
)

func main() {
	log.WithField("service", "gateway-service").Info("Starting service")

	common.SetupService("gateway-service", 50000)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	gwEnv, err := gwenv.New()
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize gateway error")
	}

	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", handler.New(gwEnv, controllers.AuthLoginHandler))
	mux.Handle("/api/auth/logout", handler.New(gwEnv, controllers.AuthLogoutHandler))

	healthz.RegisterDefaultChecks(mux)
	s := common.NewPLServer(gwEnv, mux)
	s.Start()
	s.StopOnInterrupt()
}
