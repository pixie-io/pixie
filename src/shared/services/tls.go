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

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
)

// DefaultServerTLSConfig has the TLS config setup by the default service flags.
func DefaultServerTLSConfig() (*tls.Config, error) {
	tlsCert := viper.GetString("server_tls_cert")
	tlsKey := viper.GetString("server_tls_key")
	tlsCACert := viper.GetString("tls_ca_cert")

	log.WithFields(log.Fields{
		"tlsCertFile": tlsCert,
		"tlsKeyFile":  tlsKey,
		"tlsCA":       tlsCACert,
	}).Info("Loading HTTP TLS certs")

	pair, err := tls.LoadX509KeyPair(tlsCert, tlsKey)
	if err != nil {
		return nil, fmt.Errorf("failed to load keys: %s", err.Error())
	}

	certPool := x509.NewCertPool()
	ca, err := os.ReadFile(tlsCACert)
	if err != nil {
		return nil, fmt.Errorf("failed to read CA cert: %s", err.Error())
	}

	// Append the client certificates from the CA.
	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		return nil, fmt.Errorf("failed to append CA cert")
	}

	return &tls.Config{
		Certificates: []tls.Certificate{pair},
		NextProtos:   []string{"h2"},
		ClientCAs:    certPool,
	}, nil
}
