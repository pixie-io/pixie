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
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"os"
	"time"

	"px.dev/pixie/src/e2e_test/protocol_loadtest/grpc"
	"px.dev/pixie/src/e2e_test/protocol_loadtest/http"
)

const (
	bitsize = 2048
)

var x509Name = pkix.Name{
	Organization: []string{"Pixie Labs Inc."},
	Country:      []string{"US"},
	Province:     []string{"California"},
	Locality:     []string{"San Francisco"},
}

func generateCertFilesOrDie(dnsNames []string) *tls.Config {
	ca := &x509.Certificate{
		SerialNumber:          big.NewInt(1653),
		Subject:               x509Name,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(10, 0, 0),
		IsCA:                  true,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
	}
	caKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		panic(fmt.Errorf("Error generating CA: %v", err))
	}
	cert := &x509.Certificate{
		SerialNumber:          big.NewInt(1658),
		Subject:               x509Name,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(10, 0, 0),
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		DNSNames:              dnsNames,
	}
	privateKey, err := rsa.GenerateKey(rand.Reader, bitsize)
	if err != nil {
		panic(fmt.Errorf("Error generating private key: %v", err))
	}
	certBytes, err := x509.CreateCertificate(rand.Reader, cert, ca, &privateKey.PublicKey, caKey)
	if err != nil {
		panic(fmt.Errorf("Error creating certificate: %v", err))
	}

	certData := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: certBytes})
	if err != nil {
		panic(fmt.Errorf("Error encoding cert data: %v", err))
	}

	keyData := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(privateKey)})
	if err != nil {
		panic(fmt.Errorf("Error encoding key data: %v", err))
	}

	pair, err := tls.X509KeyPair(certData, keyData)
	if err != nil {
		panic(fmt.Errorf("Error loading keypair: %v", err))
	}

	certPool := x509.NewCertPool()
	certPool.AddCert(ca)

	return &tls.Config{
		Certificates: []tls.Certificate{pair},
		ClientAuth:   tls.NoClientCert,
		RootCAs:      certPool,
	}
}

func main() {
	var tlsConfig *tls.Config

	httpPort := os.Getenv("HTTP_PORT")
	httpSSLPort := os.Getenv("HTTP_SSL_PORT")
	grpcPort := os.Getenv("GRPC_PORT")
	grpcSSLPort := os.Getenv("GRPC_SSL_PORT")

	if httpSSLPort != "" || grpcSSLPort != "" {
		tlsConfig = generateCertFilesOrDie([]string{"localhost"})
		grpcTLSConfig := tlsConfig.Clone()
		grpcTLSConfig.NextProtos = []string{"h2"}
	}

	go http.RunHTTPServers(tlsConfig, httpPort, httpSSLPort)
	grpc.RunGRPCServers(tlsConfig, grpcPort, grpcSSLPort)
}
