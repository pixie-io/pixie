package main

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io/ioutil"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"

	"px.dev/pixie/src/stirling/testing/demo_apps/go_grpc_tls_pl/server/greetpb"
)

const serverAddr = "localhost:50400"

func main() {
	pflag.String("client_tls_cert", "", "Path to client.crt")
	pflag.String("client_tls_key", "", "Path to client.key")
	pflag.String("tls_ca_cert", "", "Path to ca.crt")
	pflag.Int("count", 1000, "Number of requests sent.")
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)

	pair, err := tls.LoadX509KeyPair(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key"))
	if err != nil {
		log.WithError(err).Fatal("failed to load keys")
	}

	certPool := x509.NewCertPool()
	ca, err := ioutil.ReadFile(viper.GetString("tls_ca_cert"))
	if err != nil {
		log.WithError(err).Fatal("failed to read CA cert")
	}

	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		log.Fatal("failed to append CA cert")
	}

	config := &tls.Config{
		Certificates:       []tls.Certificate{pair},
		NextProtos:         []string{"h2"},
		ClientCAs:          certPool,
		InsecureSkipVerify: true,
	}

	conn, err := grpc.Dial(serverAddr, grpc.WithTransportCredentials(credentials.NewTLS(config)), grpc.WithBlock())
	if err != nil {
		log.WithError(err).Fatal("Failed to connect to Server.")
	}

	client := greetpb.NewGreeterClient(conn)

	for j := 0; j < viper.GetInt("count"); j++ {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()

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
