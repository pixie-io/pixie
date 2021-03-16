package main

import (
	"fmt"
	"reflect"
	"strings"
	"time"

	"github.com/fatih/color"
	"github.com/gogo/protobuf/proto"
	types "github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/msgbus"
	_ "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
)

func init() {
	pflag.Bool("filter_logs", true, "Whether or not to filter out v2c.log messages.")
	pflag.String("cluster_id", "", "The cluster id to monitor.")
	pflag.String("shard_id", "*", "The shared id to monitor.")
	pflag.StringSlice("cluster_scoped_durable_channels", []string{"DurableMetadataResponse"}, "STAN channels to watch of the form c2v.<clusterID>.<chanName>")
	pflag.StringSlice("shard_scoped_durable_channels", []string{"DurableMetadataUpdate"}, "STAN channels to watch of the form v2c.<shardID>.<chanName>")
}

// Message is the interface that both V2CMsg and C2VMsg implement.
type Message interface {
	proto.Message
	GetVizierID() string
	GetMsg() *types.Any
}

var channelTypeMap = map[string]func() Message{
	"v2c": func() Message {
		return &cvmsgspb.V2CMessage{}
	},
	"c2v": func() Message {
		return &cvmsgspb.C2VMessage{}
	},
}

const shardIDStart = 0
const shardIDEnd = 100

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

func getMsgSubjectName(fullSub string) string {
	lastDot := strings.LastIndex(fullSub, ".")
	return fullSub[lastDot+1:]
}

func handleMessage(subject string, data []byte, messageType string) {
	if viper.GetBool("filter_logs") && getMsgSubjectName(subject) == "log" {
		return
	}
	firstDot := strings.Index(subject, ".")
	if firstDot < 0 {
		log.Errorf("Could not process message from channel (reason: Invalid Channel Name): %s", subject)
		return
	}
	chanType := subject[:firstDot]
	msgFunc, ok := channelTypeMap[chanType]
	if !ok {
		log.Errorf("Failed to get message for type: %s", chanType)
		return
	}
	msg := msgFunc()
	err := proto.Unmarshal(data, msg)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal proto message.")
		return
	}

	if msg.GetMsg() == nil {
		log.Errorf("Invalid message, sub: %s, msg: %v", subject, msg)
		return
	}

	innerPb, ok := getProtoFromAny(msg.GetMsg())
	if !ok {
		return
	}

	err = proto.Unmarshal(msg.GetMsg().Value, innerPb)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal inner google.protobuf.Any message")
		return
	}
	red := color.New(color.FgRed).SprintfFunc()

	fmt.Printf("----------------------------------------------------------\n")
	fmt.Printf("%s message\n", messageType)
	fmt.Printf("%s=%s\n", red("Subject"), subject)
	fmt.Printf("%s=%s\n", red("Vizier ID"), msg.GetVizierID())
	fmt.Printf("%s\n", proto.MarshalTextString(innerPb))
	fmt.Printf("----------------------------------------------------------\n")

}

func handleSTANMessage(m *stan.Msg) {
	handleMessage(m.Subject, m.Data, "STAN")
}

func handleNATSMessage(m *nats.Msg) {
	handleMessage(m.Subject, m.Data, "NATS")
}

func possibleShardIDs() []string {
	shardIDArg := viper.GetString("shard_id")
	if shardIDArg != "*" {
		return []string{shardIDArg}
	}
	log.Info("ShardID not specified, listening on all shards...")
	possibleIDs := make([]string, 0)
	for i := shardIDStart; i <= shardIDEnd; i++ {
		// Note that the 3 in this format string represent the desired string width
		// and the 0 means to pad with 0s.
		shardID := fmt.Sprintf("%03d", i)
		possibleIDs = append(possibleIDs, shardID)
	}
	return possibleIDs
}

func subscribeToNATSChannels(nc *nats.Conn, clusterID string) {
	chanName := fmt.Sprintf("v2c.*.%s.*", clusterID)
	if _, err := nc.Subscribe(chanName, handleNATSMessage); err != nil {
		log.WithError(err).Fatal("Could not subscribe to v2c NATS channels")
	}
	log.Infof("Subscribed to NATS channel: %s", chanName)

	chanName = fmt.Sprintf("c2v.%s.*", clusterID)
	if _, err := nc.Subscribe(chanName, handleNATSMessage); err != nil {
		log.WithError(err).Fatal("Could not subscribe to c2v NATS channels")
	}
	log.Infof("Subscribed to NATS channel: %s", chanName)
}

func subscribeToSTANChannels(sc stan.Conn, clusterID string) {
	for _, channel := range viper.GetStringSlice("cluster_scoped_durable_channels") {
		chanName := fmt.Sprintf("c2v.%s.%s", clusterID, channel)
		if _, err := sc.Subscribe(chanName, handleSTANMessage, stan.DeliverAllAvailable()); err != nil {
			log.WithError(err).Fatalf("Could not subscribe to STAN channel: %s", chanName)
		}
		log.Infof("Subscribed to STAN channel: %s", chanName)
	}

	for _, shardID := range possibleShardIDs() {
		for _, channel := range viper.GetStringSlice("shard_scoped_durable_channels") {
			chanName := fmt.Sprintf("v2c.%s.%s", shardID, channel)
			if _, err := sc.Subscribe(chanName, handleSTANMessage, stan.DeliverAllAvailable()); err != nil {
				log.WithError(err).Fatalf("Could not subscribe to STAN channel: %s", chanName)
			}
			log.Infof("Subscribed to STAN channel: %s", chanName)
		}
	}
}

func main() {
	services.SetupCommonFlags()
	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckSSLClientFlags()
	clusterID := viper.GetString("cluster_id")

	if clusterID == "" {
		log.Fatal("Cluster ID not specified. Please go to k8s/utils/base/stan_watcher.yaml and set PL_CLUSTER_ID to your cluster's ID.")
	}
	log.Info("Starting STAN watcher")

	nc := msgbus.MustConnectNATS()
	sc := msgbus.MustConnectSTAN(nc, "stan-watcher")
	defer sc.Close()
	defer nc.Close()

	subscribeToNATSChannels(nc, clusterID)
	subscribeToSTANChannels(sc, clusterID)

	// Run forever to maintain subscription.
	for {
		time.Sleep(300 * time.Millisecond)
	}
}
