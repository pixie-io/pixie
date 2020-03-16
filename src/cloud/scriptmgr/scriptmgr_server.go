package main

import (
	"net/http"

	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	"pixielabs.ai/pixielabs/src/carnot/udfspb"

	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb"

	"pixielabs.ai/pixielabs/src/shared/services/env"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func main() {
	log.WithField("service", "scriptmgr-service").Info("Starting service")

	services.SetupService("scriptmgr-service", 52000)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(env.New(), mux)

	var udfInfo udfspb.UDFInfo
	planner := logicalplanner.New(&udfInfo)
	defer planner.Free()

	server := controller.NewServer(planner)
	scriptmgrpb.RegisterScriptMgrServiceServer(s.GRPCServer(), server)

	s.Start()
	s.StopOnInterrupt()
}
