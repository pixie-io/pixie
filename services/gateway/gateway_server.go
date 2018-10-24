package main

import (
	"fmt"
	"net/http"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
)

func main() {
	log.WithField("service", "gateway-service").Info("Starting service")

	common.SetupService("gateway-service", 50070)
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	handler := func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "gateway-service : %s!", r.URL.Path[1:])
	}
	mux.Handle("/", http.HandlerFunc(handler))

	healthz.RegisterDefaultChecks(mux)
	common.CreateAndRunTLSServer(mux)
}
