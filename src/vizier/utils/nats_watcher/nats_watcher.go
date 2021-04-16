package main

import (
	"fmt"
	"reflect"
	"strings"
	"time"

	"github.com/fatih/color"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services"
	messages "px.dev/pixie/src/vizier/messages/messagespb"
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

func getProtoFromAny(any *types.Any) (proto.Message, bool) {
	slash := strings.LastIndex(any.TypeUrl, "/")
	if slash < 0 {
		log.Errorf("Invalid any type url: %s", any.TypeUrl)
		return nil, false
	}
	typeName := any.TypeUrl[slash+1:]
	messageType := proto.MessageType(typeName)
	if messageType == nil {
		log.Errorf("Cannot find %s type in protobuf registries, probably missing a proto import", typeName)
	}
	pb := reflect.New(messageType.Elem()).Interface().(proto.Message)
	return pb, true
}

func handleV2CMessage(m *nats.Msg) {
	pb := &cvmsgspb.V2CMessage{}
	err := proto.Unmarshal(m.Data, pb)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal V2CMessage.")
		return
	}

	if pb.Msg == nil {
		log.Errorf("Invalid msg: %s", proto.MarshalTextString(pb))
	}
	innerPb, ok := getProtoFromAny(pb.Msg)
	if !ok {
		return
	}

	if err := proto.Unmarshal(pb.Msg.Value, innerPb); err != nil {
		log.WithError(err).Error("Failed to unmarshal inner message.")
		return
	}

	red := color.New(color.FgRed).SprintfFunc()

	fmt.Printf("----------------------------------------------------------\n")
	fmt.Printf("%s=%s\n", red("Subject"), m.Subject)
	fmt.Printf("%s\n", proto.MarshalTextString(innerPb))
	fmt.Printf("----------------------------------------------------------\n")
}

func handleNATSMessage(m *nats.Msg) {
	pb := &messages.VizierMessage{}
	err := proto.Unmarshal(m.Data, pb)

	if err != nil {
		log.WithError(err).Error("Failed to Unmarshal proto message.")
	}

	red := color.New(color.FgRed).SprintfFunc()

	fmt.Printf("----------------------------------------------------------\n")
	fmt.Printf("%s=%s\n", red("Subject"), m.Subject)
	fmt.Printf("%s\n", proto.MarshalTextString(pb))
	fmt.Printf("----------------------------------------------------------\n")
}

func main() {
	services.SetupCommonFlags()
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckSSLClientFlags()

	log.Info("Starting NATS watcher")

	nc := connectNATS()
	defer nc.Close()

	// Listen to all V2C messages.
	if _, err := nc.Subscribe("v2c.*", handleV2CMessage); err != nil {
		log.WithError(err).Fatal("Could not subscribe to NATS")
	}

	// Listen to all messages without prefixes.
	if _, err := nc.Subscribe("*", handleNATSMessage); err != nil {
		log.WithError(err).Fatal("Could not subscribe to NATS")
	}

	// Run forever to maintain subscription.
	for {
		time.Sleep(300 * time.Millisecond)
	}
}
