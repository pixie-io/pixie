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

package pxapi

import (
	"log"
	"os"
	"strings"
)

// ClientOption configures options on the client.
type ClientOption func(client *Client)

// WithCloudAddr is the option to specify cloud address to use.
func WithCloudAddr(cloudAddr string) ClientOption {
	return func(c *Client) {
		c.cloudAddr = cloudAddr
	}
}

// Allows disabling TLS verification if the `PX_DISABLE_TLS` env var is set and the cloud address is a cluster domain (cluster.local).
func WithDisableTLSVerification(cloudAddr string) ClientOption {
	return func(c *Client) {
		isInternal := strings.Contains(cloudAddr, "cluster.local")
		tlsDisabled := os.Getenv("PX_DISABLE_TLS") == "1"
		insecureSkipVerify := tlsDisabled && isInternal

		if !tlsDisabled && isInternal {
			log.Fatalf("The `PX_DISABLE_TLS` environment variable must be set to \"1\" when making cloud connections that do not support TLS.\n")
		} else {
			c.disableTLSVerification = insecureSkipVerify
		}
	}
}

// WithBearerAuth is the option to specify bearer auth to use.
func WithBearerAuth(auth string) ClientOption {
	return func(c *Client) {
		c.bearerAuth = auth
	}
}

// WithAPIKey is the option to specify the API key to use.
func WithAPIKey(auth string) ClientOption {
	return func(c *Client) {
		c.apiKey = auth
	}
}

// WithE2EEncryption is the option to enable E2E ecnryption for table data.
func WithE2EEncryption(enabled bool) ClientOption {
	return func(c *Client) {
		c.useEncryption = enabled
	}
}

// WithDirectAddr is the option to specify direct address to use for data from standalone pem.
func WithDirectAddr(directAddr string) ClientOption {
	return func(c *Client) {
		c.directAddr = directAddr
	}
}
