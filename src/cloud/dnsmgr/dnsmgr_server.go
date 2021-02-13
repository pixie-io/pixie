package main

import (
	"net/http"

	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/controller"
	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrenv"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/schema"
	"pixielabs.ai/pixielabs/src/shared/services/pg"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("dns_zone", "cluster-dev-withpixie-dev", "The zone to use for cloud DNS")
	pflag.String("dns_project", "pl-dev-infra", "The project to use for cloud DNS")
	pflag.String("domain_name", "withpixie.ai", "The domain name")
	pflag.Bool("use_default_dns_cert", false, "Whether to use the default DNS ssl cert")
}

func main() {
	services.SetupService("dnsmgr-service", 51900)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	db := pg.MustConnectDefaultPostgresDB()

	// TODO(zasgar): Pull out this migration code into a util. Just leaving it here for now for testing.
	driver, err := postgres.WithInstance(db.DB, &postgres.Config{
		MigrationsTable: "dnsmgr_service_migrations",
	})

	sc := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	d, err := bindata.WithInstance(sc)

	mg, err := migrate.NewWithInstance(
		"go-bindata",
		d, "postgres", driver)

	if err = mg.Up(); err != nil {
		log.WithError(err).Info("migrations failed: %s", err)
	}

	env := dnsmgrenv.New()

	dnsService, err := controller.NewCloudDNSService(
		viper.GetString("dns_zone"),
		viper.GetString("dns_project"),
		"/secrets/clouddns/dns_service_account.json",
	)

	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Cloud DNS service")
	}

	svr := controller.NewServer(env, dnsService, db)

	s := server.NewPLServer(env, mux)
	dnsmgrpb.RegisterDNSMgrServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
