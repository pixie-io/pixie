package main

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"net/http"

	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	"github.com/olivere/elastic/v7"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/indexer/controllers"
	"pixielabs.ai/pixielabs/src/cloud/indexer/md"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/healthz"
	"pixielabs.ai/pixielabs/src/shared/services/server"
)

func init() {
	pflag.String("nats_url", "pl-nats", "The URL of NATS")
	pflag.String("stan_cluster", "pl-stan", "The name of the STAN cluster")
	pflag.String("es_url", "https://pl-elastic-es-http:9200", "The URL for the elastic cluster")
	pflag.String("es_ca_cert", "/es-certs/tls.crt", "The CA cert for elastic")
	pflag.String("es_user", "elastic", "The user for elastic")
	pflag.String("es_passwd", "elastic", "The password for elastic")
	pflag.String("vzmgr_service", "kubernetes:///vzmgr-service.plc:51800", "The profile service url (load balancer/list is ok)")
}

func newVZMgrClient() (vzmgrpb.VZMgrServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	vzmgrChannel, err := grpc.Dial(viper.GetString("vzmgr_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return vzmgrpb.NewVZMgrServiceClient(vzmgrChannel), nil
}

func createStanNatsConnection(clientID string) (nc *nats.Conn, sc stan.Conn, err error) {
	nc, err = nats.Connect(viper.GetString("nats_url"),
		nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
		nats.RootCAs(viper.GetString("tls_ca_cert")))
	if err != nil {
		log.WithError(err).Error("NATS connection failed")
		return
	}
	sc, err = stan.Connect(viper.GetString("stan_cluster"),
		clientID, stan.NatsConn(nc),
		stan.SetConnectionLostHandler(func(_ stan.Conn, err error) {
			log.WithError(err).Fatal("STAN Connection Lost")
		}))
	if err != nil {
		log.WithError(err).Error("STAN connection failed")
	}
	return
}

func getESHTTPSClient() (*http.Client, error) {
	caFile := viper.GetString("es_ca_cert")
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

func mustConnectElastic() *elastic.Client {
	esURL := viper.GetString("es_url")
	httpClient, err := getESHTTPSClient()
	if err != nil {
		log.WithError(err).Fatal("Failed to create HTTPS client")
	}
	es, err := elastic.NewClient(elastic.SetURL(esURL),
		elastic.SetHttpClient(httpClient),
		elastic.SetBasicAuth(viper.GetString("es_user"), viper.GetString("es_passwd")),
		// Sniffing seems to be broken with TLS, don't turn this on unless you want pain.
		elastic.SetSniff(false))
	if err != nil {
		log.WithError(err).Fatalf("Failed to connect to elastic at url: %s", esURL)
	}
	return es
}

func main() {
	services.SetupService("indexer-service", 51800)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	healthz.RegisterDefaultChecks(mux)

	s := server.NewPLServer(env.New(), mux)
	nc, sc, err := createStanNatsConnection(uuid.NewV4().String())
	if err != nil {
		log.Fatal("Could not connect to NATS/STAN")
	}

	nc.SetErrorHandler(func(conn *nats.Conn, subscription *nats.Subscription, err error) {
		log.WithError(err).
			WithField("sub", subscription.Subject).
			Error("Got nats error")
	})

	es := mustConnectElastic()
	err = md.InitializeMapping(es)
	if err != nil {
		log.WithError(err).Fatal("Could not intialize elastic mapping")
	}

	vzmgrClient, err := newVZMgrClient()
	if err != nil {
		log.WithError(err).Fatal("Could not connect to vzmgr")
	}

	indexer, err := controllers.NewIndexer(nc, vzmgrClient, sc, es, "00", "ff")
	if err != nil {
		log.WithError(err).Fatal("Could not start indexer")
	}

	defer indexer.Stop()

	s.Start()
	s.StopOnInterrupt()
}
