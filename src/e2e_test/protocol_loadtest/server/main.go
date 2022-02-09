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
	"strconv"
	"time"

	"px.dev/pixie/src/e2e_test/protocol_loadtest/grpc"
	"px.dev/pixie/src/e2e_test/protocol_loadtest/http"
)

const (
	bitsize  = 4096
	certFile = "server.crt"
	keyFile  = "server.key"

	defaultHTTPNumBytesHeaders = 1024
	defaultHTTPNumBytesBody    = 1024
)

var x509Name = pkix.Name{
	Organization: []string{"Pixie Labs Inc."},
	Country:      []string{"US"},
	Province:     []string{"California"},
	Locality:     []string{"San Francisco"},
}

func generateCertFilesOrDie(dnsNames []string) (string, string, *tls.Config) {
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
	if err = os.WriteFile(certFile, certData, 0666); err != nil {
		panic(fmt.Errorf("Error writing cert to %s: %v", certFile, err))
	}

	keyData := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(privateKey)})
	if err != nil {
		panic(fmt.Errorf("Error encoding key data: %v", err))
	}
	if err = os.WriteFile(keyFile, keyData, 0666); err != nil {
		panic(fmt.Errorf("Error writing key to %s: %v", keyFile, err))
	}

	pair, err := tls.LoadX509KeyPair(certFile, keyFile)
	if err != nil {
		panic(fmt.Errorf("Error loading keypair from %s and %s: %v", certFile, keyFile, err))
	}

	certPool := x509.NewCertPool()
	certPool.AddCert(ca)

	tlsConfig := &tls.Config{
		Certificates: []tls.Certificate{pair},
		ClientAuth:   tls.NoClientCert,
		NextProtos:   []string{"h2"},
		RootCAs:      certPool,
	}
	return certFile, keyFile, tlsConfig
}

func main() {
	var certFile string
	var keyFile string
	var tlsConfig *tls.Config

	httpPort := os.Getenv("HTTP_PORT")
	httpSSLPort := os.Getenv("HTTP_SSL_PORT")
	grpcPort := os.Getenv("GRPC_PORT")
	grpcSSLPort := os.Getenv("GRPC_SSL_PORT")

	if httpSSLPort != "" || grpcSSLPort != "" {
		certFile, keyFile, tlsConfig = generateCertFilesOrDie([]string{"localhost"})
	}

	httpNumBytesHeaders, err := strconv.Atoi(os.Getenv("HTTP_NUM_BYTES_HEADERS"))
	if err != nil {
		httpNumBytesHeaders = defaultHTTPNumBytesHeaders
	}
	httpNumBytesBody, err := strconv.Atoi(os.Getenv("HTTP_NUM_BYTES_BODY"))
	if err != nil {
		httpNumBytesBody = defaultHTTPNumBytesBody
	}

	go http.RunHTTPServers(certFile, keyFile, httpPort, httpSSLPort, httpNumBytesHeaders, httpNumBytesBody)
	grpc.RunGRPCServers(tlsConfig, grpcPort, grpcSSLPort)
}
