package main

import (
	"fmt"
	"log"
	"runtime"
	"time"

	"crypto/tls"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	pb "pixielabs.ai/pixielabs/src/stirling/http2/testing/proto"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const (
	address = "127.0.0.1:50051"
)

func main() {
	pflag.Int("max_procs", 1, "The maximum number of OS threads created by the golang runtime.")
	pflag.Int("iters", 1000, "Number of iterations.")
	pflag.Int("sub_iters", 1000, "Number of sub-iterations with same TLS config.")
	pflag.Parse()

	viper.BindPFlags(pflag.CommandLine)

	runtime.GOMAXPROCS(viper.GetInt("max_procs"))

	for i := 0; i < viper.GetInt("iters"); i++ {
		// Set up a connection to the server.
		tlsConfig := &tls.Config{InsecureSkipVerify: true}
		creds := credentials.NewTLS(tlsConfig)
		conn, err := grpc.Dial(address, grpc.WithTransportCredentials(creds))
		if err != nil {
			log.Fatalf("did not connect: %v", err)
		}
		defer conn.Close()
		c := pb.NewGreeterClient(conn)

		for j := 0; j < viper.GetInt("sub_iters"); j++ {
			ctx, cancel := context.WithTimeout(context.Background(), time.Second)
			defer cancel()

			name := fmt.Sprintf("world %d", j)
			resp, err := c.SayHello(ctx, &pb.HelloRequest{Name: name})
			if err != nil {
				log.Fatalf("could not greet: %v", err)
			} else {
				log.Printf("Greeting: %s", resp.Message)
			}
			time.Sleep(time.Second)
		}
	}
}
