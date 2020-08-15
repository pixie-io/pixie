package main

import (
	"context"
	"net/http"

	"github.com/nats-io/nats.go"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	log "github.com/sirupsen/logrus"
	logmessagehandler "pixielabs.ai/pixielabs/src/cloud/log_collector/log_message_handler"
	"pixielabs.ai/pixielabs/src/cloud/shared/esutils"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

// Note: we have to force the mapping to use "text" for the time field
// in log_processed, because some logs have time in date format and some
// have time in "%dms" format.
const indexSpec = `
{
    "settings": {
      "number_of_shards": 10,
      "number_of_replicas": 2,
    },
	"mappings" : {
		"properties" : {
			"log_processed" : {
				"properties" : {
					"time" : {
						"type" : "text"
					}
				}
			}
		}
	}
}`

func init() {
	pflag.String("nats_url", "pl-nats", "The url of the nats message bus")
	pflag.String("elastic_service", "https://pl-elastic-es-http.plc-dev.svc.cluster.local:9200", "The url of the elasticsearch cluster")
	pflag.String("elastic_ca_cert", "/elastic-certs/ca.crt", "CA Cert for elastic cluster")
	pflag.String("elastic_tls_cert", "/elastic-certs/tls.crt", "TLS Cert for elastic cluster")
	pflag.String("elastic_tls_key", "/elastic-certs/tls.key", "TLS Key for elastic cluster")
	pflag.String("elastic_username", "elastic", "Username for access to elastic cluster")
	pflag.String("elastic_password", "", "Password for access to elastic")
	pflag.String("logs_index_name", "vizier-logs-allclusters", "Name of managed index to use for logs.")
	pflag.String("logs_max_index_size", "50mb", "Maximum size the log index is allowed to grow to before its rolled over.")
	pflag.String("logs_time_before_delete", "1d", "Time to keep rolledover logs around before deletion.")
}

func connectNATS(natsURL string) *nats.Conn {
	var nc *nats.Conn
	var err error
	if viper.GetBool("disable_ssl") {
		nc, err = nats.Connect(natsURL)
	} else {
		nc, err = nats.Connect(natsURL,
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}

	if err != nil && !viper.GetBool("disable_ssl") {
		log.WithError(err).
			WithField("client_tls_cert", viper.GetString("client_tls_cert")).
			WithField("client_tls_key", viper.GetString("client_tls_key")).
			WithField("tls_ca_cert", viper.GetString("tls_ca_cert")).
			Fatal("Failed to connect to NATS")
	} else if err != nil {
		log.WithError(err).Fatal("Failed to connect to NATS")
	}
	return nc
}

func main() {
	log.WithField("service", "log-collector-service").Info("Starting service")

	services.SetupService("log-collector-service", 52500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	esConfig := &esutils.Config{
		URL:        []string{viper.GetString("elastic_service")},
		User:       viper.GetString("elastic_username"),
		Passwd:     viper.GetString("elastic_password"),
		CaCertFile: viper.GetString("elastic_ca_cert"),
	}
	es, err := esutils.NewEsClient(esConfig)
	if err != nil {
		log.WithError(err).Fatal("Could not connect to elastic")
	}

	indexName := viper.GetString("logs_index_name")
	maxIndexSize := viper.GetString("logs_max_index_size")
	timeBeforeDelete := viper.GetString("logs_time_before_delete")
	err = esutils.NewManagedIndex(es, indexName).
		IndexFromJSONString(indexSpec).
		MaxIndexSize(maxIndexSize).
		TimeBeforeDelete(timeBeforeDelete).
		Migrate(context.Background())
	if err != nil {
		log.WithError(err).Fatal("failed to migrate ManagedIndex")
	}

	nc := connectNATS(viper.GetString("nats_url"))
	h := logmessagehandler.NewLogMessageHandler(context.Background(), nc, es, indexName)
	h.Start()

	env := env.New()
	s := services.NewPLServer(env, mux)
	s.Start()
	s.StopOnInterrupt()
}
