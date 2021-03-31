package main

import (
	"context"
	"fmt"
	"testing"
	"time"

	log "github.com/sirupsen/logrus"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/stirling/testing/demo_apps/go_grpc_tls_pl/server/greetpb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

const serverAddr = "localhost:50400"
const serverJWTSigningKey = "123456"

func main() {
	pflag.Int("count", 1000, "Number of requests sent.")

	services.SetupSSLClientFlags()
	services.PostFlagSetupAndParse()
	services.CheckSSLClientFlags()

	// Connect to server.
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		log.WithError(err).Fatal("Could not get dial opts.")
	}
	dialOpts = append(dialOpts, grpc.WithBlock())

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	conn, err := grpc.DialContext(ctx, serverAddr, dialOpts...)
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Server.")
	}
	client := greetpb.NewGreeterClient(conn)

	token := testingutils.GenerateTestJWTToken(&testing.T{}, serverJWTSigningKey)

	for j := 0; j < viper.GetInt("count"); j++ {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()

		ctx = metadata.AppendToOutgoingContext(ctx, "Authorization", "bearer "+token)

		name := fmt.Sprintf("%d", j)
		resp, err := client.SayHello(ctx, &greetpb.HelloRequest{Name: name})
		if err != nil {
			log.Printf("could not greet: %v", err)
		} else {
			log.Printf("Greeting: %s", resp.Message)
		}
		time.Sleep(time.Second)
	}
}
