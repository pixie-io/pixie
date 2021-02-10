package main

import (
	"context"
	"net/http"

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
	healthz.RegisterDefaultChecks(mux)

	s := services.NewPLServer(env.New(), mux)

	client, err := storage.NewClient(context.Background())
	if err != nil {
		log.WithError(err).Fatal("Failed to initialize GCS client.")
	}

	server := controller.NewServer(
		viper.GetString("bundle_bucket"),
		viper.GetString("bundle_path"),
		stiface.AdaptClient(client))
	server.Start()

	scriptmgrpb.RegisterScriptMgrServiceServer(s.GRPCServer(), server)

	s.Start()
	s.StopOnInterrupt()
}
