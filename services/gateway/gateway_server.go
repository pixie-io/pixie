package main

import (
	"net/http"

	"github.com/gorilla/context"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
	"pixielabs.ai/pixielabs/services/gateway/controllers"
)

func main() {
	log.WithField("service", "gateway-service").Info("Starting service")

	common.SetupService("gateway-service", 50070)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", http.HandlerFunc(controllers.AuthLoginHandler))
	mux.Handle("/api/auth/logout", http.HandlerFunc(controllers.AuthLogoutHandler))

	healthz.RegisterDefaultChecks(mux)
	common.CreateAndRunTLSServer(context.ClearHandler(WithEnvMiddleware(mux)))
}
