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
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"os"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

var (
	subj = pkix.Name{
		Organization: []string{"Pixie Labs Inc."},
		Country:      []string{"US"},
		Province:     []string{"California"},
		Locality:     []string{"San Francisco"},
	}
)

// generateAndWriteCA generates a CA cert and writes it to the file pointed by the ca_crt flag.
// It also returns the generated cert and privateKey so that they can be used to generate and sign
// other cert/key pairs.
func generateAndWriteCA() (*x509.Certificate, *rsa.PrivateKey) {
	ca := &x509.Certificate{
		SerialNumber:          big.NewInt(123456),
		Subject:               subj,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(1, 0, 0),
		IsCA:                  true,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
	}
	caKey, err := rsa.GenerateKey(rand.Reader, 4096)
	if err != nil {
		panic(err)
	}
	caCertBytes, err := x509.CreateCertificate(rand.Reader, ca, ca, &caKey.PublicKey, caKey)
	if err != nil {
		panic(err)
	}
	caCertOut, err := os.OpenFile(viper.GetString("ca_crt"), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		panic(err)
	}
	err = pem.Encode(caCertOut, &pem.Block{Type: "CERTIFICATE", Bytes: caCertBytes})
	if err != nil {
		panic(err)
	}
	err = caCertOut.Close()
	if err != nil {
		panic(err)
	}
	return ca, caKey
}

// generateAndWriteCertPair uses the given CA to generate a cert/key pair and write those to the given filepaths.
func generateAndWriteCertPair(ca *x509.Certificate, caKey *rsa.PrivateKey, certPath, keyPath string) {
	cert := &x509.Certificate{
		SerialNumber:          big.NewInt(654321),
		Subject:               subj,
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(1, 0, 0),
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth, x509.ExtKeyUsageServerAuth},
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		BasicConstraintsValid: true,
		DNSNames:              []string{"localhost"},
	}
	privateKey, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		panic(err)
	}
	certBytes, err := x509.CreateCertificate(rand.Reader, cert, ca, &privateKey.PublicKey, caKey)
	if err != nil {
		panic(err)
	}
	certOut, err := os.OpenFile(certPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		panic(err)
	}
	err = pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: certBytes})
	if err != nil {
		panic(err)
	}
	err = certOut.Close()
	if err != nil {
		panic(err)
	}
	keyOut, err := os.OpenFile(keyPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		panic(err)
	}

	keyType := viper.GetString("secret_key_type")
	if keyType == "pkcs1" {
		err = pem.Encode(keyOut, &pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(privateKey)})
		if err != nil {
			panic(err)
		}
	} else if keyType == "pkcs8" {
		b, err := x509.MarshalPKCS8PrivateKey(privateKey)
		if err != nil {
			panic(err)
		}

		err = pem.Encode(keyOut, &pem.Block{Type: "PRIVATE KEY", Bytes: b})
		if err != nil {
			panic(err)
		}
	} else {
		panic(fmt.Sprintf("Unsupported private key type: %s", keyType))
	}
	err = keyOut.Close()
	if err != nil {
		panic(err)
	}
}

func main() {
	pflag.String("ca_crt", "", "Path where to write ca.crt")
	pflag.String("client_crt", "", "Path where to write client.crt")
	pflag.String("client_key", "", "Path where to write client.key")
	pflag.String("server_crt", "", "Path where to write server.crt")
	pflag.String("server_key", "", "Path where to write server.key")
	pflag.String("secret_key_type", "pkcs1", "Which private key type to use. Possible options include pkcs1, pkcs8")

	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)

	ca, caKey := generateAndWriteCA()
	generateAndWriteCertPair(ca, caKey, viper.GetString("server_crt"), viper.GetString("server_key"))
	generateAndWriteCertPair(ca, caKey, viper.GetString("client_crt"), viper.GetString("client_key"))
}
