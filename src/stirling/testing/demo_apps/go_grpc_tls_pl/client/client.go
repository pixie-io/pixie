/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package main

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"os"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"

	"px.dev/pixie/src/stirling/testing/demo_apps/go_grpc_tls_pl/server/greetpb"
)

func main() {
	pflag.String("client_tls_cert", "", "Path to client.crt")
	pflag.String("client_tls_key", "", "Path to client.key")
	pflag.String("tls_ca_cert", "", "Path to ca.crt")
	pflag.String("address", "localhost:50400", "Server address")
	pflag.Int("count", 1000, "Number of requests sent.")
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)

	pair, err := tls.LoadX509KeyPair(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key"))
	if err != nil {
		log.WithError(err).Fatal("failed to load keys")
	}

	certPool := x509.NewCertPool()
	ca, err := os.ReadFile(viper.GetString("tls_ca_cert"))
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

	conn, err := grpc.Dial(viper.GetString("address"), grpc.WithTransportCredentials(credentials.NewTLS(config)), grpc.WithBlock())
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
