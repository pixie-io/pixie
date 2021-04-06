package main

import (
	"context"
	"net/http"
	_ "net/http/pprof"

	"cloud.google.com/go/storage"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("bundle_bucket", "pixie-prod-artifacts", "GCS Bucket containing the bundle of scripts.")
	pflag.String("bundle_path", "script-bundles/bundle.json", "Path to bundle within bucket.")
}

func main() {
	services.SetupService("scriptmgr-service", 52000)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	s := server.NewPLServer(env.New(viper.GetString("domain_name")), mux)

	client, err := storage.NewClient(context.Background())
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GCS client.")
	}

	svr := controller.NewServer(
		viper.GetString("bundle_bucket"),
		viper.GetString("bundle_path"),
		stiface.AdaptClient(client))
	svr.Start()

	scriptmgrpb.RegisterScriptMgrServiceServer(s.GRPCServer(), svr)

	s.Start()
	s.StopOnInterrupt()
}
