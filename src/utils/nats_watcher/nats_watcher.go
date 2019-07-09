package main

import (
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/services/common"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
)

func connectNATS() *nats.Conn {
	natsURL := "pl-nats"

	var nc *nats.Conn
	var err error
	if viper.GetBool("disable_ssl") {
		nc, err = nats.Connect(natsURL)
	} else {
		nc, err = nats.Connect(natsURL,
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}

	if err != nil && viper.GetBool("disable_ssl") {
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

func handleNATSMessage(m *nats.Msg) {
	pb := &messages.VizierMessage{}
	proto.Unmarshal(m.Data, pb)
	log.WithField("Subject", string(m.Subject)).WithField("Message", (*pb).String()).Info("Received message")
}

func main() {
	common.SetupCommonFlags()
	common.SetupSSLClientFlags()
	common.PostFlagSetupAndParse()
	common.CheckSSLClientFlags()

	log.Info("Starting NATS watcher")

	nc := connectNATS()
	defer nc.Close()

	// Listen to all messages.
	if _, err := nc.Subscribe("*", handleNATSMessage); err != nil {
		log.WithError(err).Fatal("Could not subscribe to NATS")
	}

	// Run forever to maintain subscription.
	for {
		time.Sleep(300 * time.Millisecond)
	}
}
