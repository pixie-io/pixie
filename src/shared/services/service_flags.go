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

package services

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"os"
	"sync"

	"github.com/sercand/kuberesolver/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/encoding/gzip"

	version "px.dev/pixie/src/shared/goversion"
)

var (
	commonSetup sync.Once
)

func init() {
	// Enable the k8s DNS resolver to lookup services.
	kuberesolver.RegisterInCluster()
}

func setupCommonFlags() {
	pflag.Bool("disable_ssl", false, "Disable SSL on the server")
	pflag.Bool("disable_grpc_auth", false, "Disable auth on the GRPC server")
	pflag.String("tls_ca_cert", "../certs/ca.crt", "The CA cert.")
	pflag.String("jwt_signing_key", "", "The signing key used for JWTs")
	pflag.String("pod_name", "<unknown>", "The pod name")
	pflag.Bool("version", false, "Print the version and quit.")
}

// SetupCommonFlags sets flags that are used by every service, even non GRPC servers.
func SetupCommonFlags() {
	commonSetup.Do(setupCommonFlags)
}

// SetupService configures basic flags and defaults required by all services.
func SetupService(serviceName string, servicePortBase uint) {
	commonSetup.Do(setupCommonFlags)
	pflag.Uint("http2_port", servicePortBase, fmt.Sprintf("The port to run the %s HTTP/2 server", serviceName))
	pflag.Uint("metrics_http_port", servicePortBase+1, fmt.Sprintf("The port to run the %s HTTP metrics server", serviceName))
	pflag.String("server_tls_key", "../certs/server.key", "The TLS key to use.")
	pflag.String("server_tls_cert", "../certs/server.crt", "The TLS certificate to use.")

	log.WithField("service", serviceName).
		WithField("version", version.GetVersion().ToString()).
		Info("Starting service")
}

// PostFlagSetupAndParse does post setup flag config and parses them.
func PostFlagSetupAndParse() {
	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)
}

// CheckServiceFlags checks to make sure flag values are valid.
func CheckServiceFlags() {
	if viper.GetBool("version") {
		log.WithField("version", version.GetVersion().ToString()).
			Info("Exiting")
		os.Exit(0)
	}

	if len(viper.GetString("jwt_signing_key")) == 0 {
		log.Panic("Flag --jwt_signing_key or ENV PL_JWT_SIGNING_KEY is required")
	}

	if !viper.GetBool("disable_ssl") {
		if len(viper.GetString("server_tls_key")) == 0 {
			log.Panic("Flag --server_tls_key or ENV PL_SERVER_TLS_KEY is required when ssl is enabled")
		}

		if len(viper.GetString("server_tls_cert")) == 0 {
			log.Panic("Flag --server_tls_cert or ENV PL_SERVER_TLS_CERT is required when ssl is enabled")
		}

		if len(viper.GetString("tls_ca_cert")) == 0 {
			log.Panic("Flag --tls_ca_cert or ENV PL_TLS_CA_CERT is required when ssl is enabled")
		}
	}

	if viper.GetBool("disable_grpc_auth") {
		log.Warn("Security WARNING!!! : Auth disabled on GRPC.")
	}
}

// SetupSSLClientFlags sets up SSL client specific flags.
func SetupSSLClientFlags() {
	commonSetup.Do(setupCommonFlags)
	pflag.String("client_tls_key", "../certs/client.key", "The TLS key to use.")
	pflag.String("client_tls_cert", "../certs/client.crt", "The TLS certificate to use.")
}

// CheckSSLClientFlags checks SSL client specific flags.
func CheckSSLClientFlags() {
	if !viper.GetBool("disable_ssl") {
		if len(viper.GetString("client_tls_key")) == 0 {
			log.Panic("Flag --client_tls_key or ENV PL_CLIENT_TLS_KEY is required when ssl is enabled")
		}

		if len(viper.GetString("client_tls_cert")) == 0 {
			log.Panic("Flag --client_tls_cert or ENV PL_CLIENT_TLS_CERT is required when ssl is enabled")
		}

		if len(viper.GetString("tls_ca_cert")) == 0 {
			log.Panic("Flag --tls_ca_cert or ENV PL_TLS_CA_CERT is required when ssl is enabled")
		}
	}
}

// GetGRPCClientDialOpts gets default dial options for GRPC clients used for our services.
func GetGRPCClientDialOpts() ([]grpc.DialOption, error) {
	dialOpts := make([]grpc.DialOption, 0)
	dialOpts = append(dialOpts, grpc.WithDefaultCallOptions(grpc.UseCompressor(gzip.Name)))

	if viper.GetBool("disable_ssl") {
		dialOpts = append(dialOpts, grpc.WithTransportCredentials(insecure.NewCredentials()))
		return dialOpts, nil
	}

	tlsCert := viper.GetString("client_tls_cert")
	tlsKey := viper.GetString("client_tls_key")
	tlsCACert := viper.GetString("tls_ca_cert")

	log.WithFields(log.Fields{
		"tlsCertFile": tlsCert,
		"tlsKeyFile":  tlsKey,
		"tlsCA":       tlsCACert,
	}).Info("Loading HTTP TLS certs")

	pair, err := tls.LoadX509KeyPair(tlsCert, tlsKey)
	if err != nil {
		return nil, err
	}

	certPool := x509.NewCertPool()
	ca, err := os.ReadFile(tlsCACert)
	if err != nil {
		return nil, err
	}

	// Append the client certificates from the CA
	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		return nil, fmt.Errorf("failed to append CA cert: %s", tlsCACert)
	}

	tlsConfig := &tls.Config{
		Certificates: []tls.Certificate{pair},
		NextProtos:   []string{"h2"},
		RootCAs:      certPool,
	}

	creds := credentials.NewTLS(tlsConfig)
	dialOpts = append(dialOpts, grpc.WithTransportCredentials(creds))
	dialOpts = append(dialOpts, grpc.WithDefaultServiceConfig(`{"loadBalancingPolicy":"round_robin"}`))

	return dialOpts, nil
}

// GetGRPCClientDialOptsServerSideTLS gets default dial options for GRPC clients accessing a server with server-side TLS.
func GetGRPCClientDialOptsServerSideTLS(isInternal bool) ([]grpc.DialOption, error) {
	dialOpts := make([]grpc.DialOption, 0)
	dialOpts = append(dialOpts, grpc.WithDefaultCallOptions(grpc.UseCompressor(gzip.Name)))

	if viper.GetBool("disable_ssl") {
		dialOpts = append(dialOpts, grpc.WithTransportCredentials(insecure.NewCredentials()))
		return dialOpts, nil
	}

	tlsConfig := &tls.Config{InsecureSkipVerify: isInternal}
	creds := credentials.NewTLS(tlsConfig)

	dialOpts = append(dialOpts, grpc.WithTransportCredentials(creds))
	return dialOpts, nil
}
