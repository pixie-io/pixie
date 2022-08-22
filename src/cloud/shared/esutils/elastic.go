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

package esutils

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"net"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/olivere/elastic/v7"
)

// Config describes the underlying config for elastic.
type Config struct {
	URL        []string `json:"url"`
	User       string   `json:"user"`
	Passwd     string   `json:"passwd"`
	CaCertFile string   `json:"ca_cert_file"`
}

func getESHTTPSClient(config *Config) (*http.Client, error) {
	caCert, err := os.ReadFile(config.CaCertFile)
	if err != nil {
		return nil, err
	}
	caCertPool := x509.NewCertPool()
	ok := caCertPool.AppendCertsFromPEM(caCert)
	if !ok {
		return nil, fmt.Errorf("failed to append caCert to pool")
	}
	tlsConfig := &tls.Config{
		RootCAs: caCertPool,
	}

	tr := http.DefaultTransport.(*http.Transport).Clone()
	tr.TLSClientConfig = tlsConfig
	tr.MaxIdleConnsPerHost = 100 // Default is 2.
	tr.DialContext = (&net.Dialer{KeepAlive: 30 * time.Second}).DialContext

	httpClient := &http.Client{
		Transport: tr,
	}
	return httpClient, nil
}

func setupHTTPS(config *Config) ([]elastic.ClientOptionFunc, error) {
	var opts []elastic.ClientOptionFunc
	httpClient, err := getESHTTPSClient(config)
	if err != nil {
		return opts, err
	}
	opts = append(opts,
		elastic.SetHttpClient(httpClient))
	return opts, nil
}

// NewEsClient creates an elastic search client from the config.
func NewEsClient(config *Config) (*elastic.Client, error) {
	var opts []elastic.ClientOptionFunc

	// Sniffer should look for HTTPS URLs if at-least-one initial URL is HTTPS
	for _, url := range config.URL {
		if strings.HasPrefix(url, "https:") {
			httpsOpts, err := setupHTTPS(config)
			if err != nil {
				return nil, err
			}
			opts = append(opts, httpsOpts...)
			break
		}
	}

	opts = append(opts, elastic.SetURL(config.URL...), elastic.SetBasicAuth(config.User, config.Passwd), elastic.SetSniff(false))

	return elastic.NewClient(opts...)
}
