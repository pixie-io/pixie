package main

import (
	"fmt"
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/api_service/controller"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
)

func main() {
	log.WithField("service", "api-service").Info("Starting service")

	common.SetupService("api-service", 50050)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	handler := func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "Hi there @ path : %s!", r.URL.Path[1:])
	}
	mux.Handle("/", http.HandlerFunc(handler))
	mux.Handle("/gql", controller.GraphQLHandler())
	healthz.RegisterDefaultChecks(mux)

	common.CreateAndRunTLSServer(mux)
}
