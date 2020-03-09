package main

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"net/http"

	"github.com/nats-io/nats.go"

	"github.com/olivere/elastic/v7"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	log "github.com/sirupsen/logrus"
	logmessagehandler "pixielabs.ai/pixielabs/src/cloud/log_collector/log_message_handler"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
)

func init() {
	pflag.String("nats_url", "pl-nats", "The url of the nats message bus")
	pflag.String("elastic_service", "https://pl-elastic-es-http.plc-dev.svc.cluster.local:9200", "The url of the elasticsearch cluster")
	pflag.String("elastic_ca_cert", "/elastic-certs/ca.crt", "CA Cert for elastic cluster")
	pflag.String("elastic_tls_cert", "/elastic-certs/tls.crt", "TLS Cert for elastic cluster")
	pflag.String("elastic_tls_key", "/elastic-certs/tls.key", "TLS Key for elastic cluster")
	pflag.String("elastic_username", "elastic", "Username for access to elastic cluster")
	pflag.String("elastic_password", "", "Password for access to elastic")
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

func getHTTPSClient() (*http.Client, error) {
	caFile := viper.GetString("elastic_ca_cert")

	caCert, err := ioutil.ReadFile(caFile)
	if err != nil {
		return nil, err
	}
	caCertPool := x509.NewCertPool()
	ok := caCertPool.AppendCertsFromPEM(caCert)
	if !ok {
		return nil, fmt.Errorf("failed to append caCert to pool")
	}

	tlsConfig := &tls.Config{
		RootCAs: caCertPool,
	}
	tlsConfig.BuildNameToCertificate()
	transport := &http.Transport{
		TLSClientConfig: tlsConfig,
	}

	httpClient := &http.Client{
		Transport: transport,
	}
	return httpClient, nil
}

func connectElastic(esURL string, esUser string, esPass string) *elastic.Client {
	httpClient, err := getHTTPSClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to create HTTPS client")
	}

	es, err := elastic.NewClient(elastic.SetURL(esURL),
		elastic.SetHttpClient(httpClient),
		elastic.SetBasicAuth(esUser, esPass),
		// Sniffing seems to be broken with TLS, don't turn this on unless you want pain.
		elastic.SetSniff(false))
	if err != nil {
		log.WithError(err).Fatalf("Failed to connect to elastic at url: %s", esURL)
	}
	return es
}

func main() {
	log.WithField("service", "log-collector-service").Info("Starting service")

	services.SetupService("log-collector-service", 52500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	es := connectElastic(viper.GetString("elastic_service"),
		viper.GetString("elastic_username"), viper.GetString("elastic_password"))
	nc := connectNATS(viper.GetString("nats_url"))
	h := logmessagehandler.NewLogMessageHandler(context.Background(), nc, es)
	h.Start()

	env := env.New()
	s := services.NewPLServer(env, mux)
	s.Start()
	s.StopOnInterrupt()
}
