package main

import (
	"fmt"
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/auth/controllers"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
)

func main() {
	log.WithField("service", "auth-service").Info("Starting service")

	common.SetupService("auth-service", 50060)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	env := &controllers.AuthEnv{}

	common.CreateGrpcServer(env)

	mux := http.NewServeMux()
	handler := func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "Hi there @ path : %s!", r.URL.Path[1:])
	}
	mux.Handle("/", http.HandlerFunc(handler))

	loginHandler, err := controllers.NewHandleLoginFunc()

	if err != nil {
		log.WithError(err).Panic("could not initialize login function")
	}

	mux.Handle("/auth/login", loginHandler)
	healthz.RegisterDefaultChecks(mux)

	common.CreateAndRunTLSServer(mux)
}
