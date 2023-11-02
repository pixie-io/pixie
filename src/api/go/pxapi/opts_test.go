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
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestWithDisableTLSVerificationOnlyAppliesForLocalConn(t *testing.T) {
	if err := os.Setenv("PX_DISABLE_TLS", "1"); err != nil {
		t.Fatalf("failed to set the `PX_DISABLE_TLS` environment variable: %v", err)
	}
	c := &Client{}
	// Hostname that isn't a cluster domain (cluster.local or custom domain)
	opt := WithDisableTLSVerification("work.withpixie.ai")
	opt(c)

	assert.False(t, c.disableTLSVerification)

	if err := os.Unsetenv("PX_DISABLE_TLS"); err != nil {
		t.Fatalf("failed to unset the `PX_DISABLE_TLS` environment variable: %v", err)
	}
}

func TestWithDisableTLSVerificationWithEnvVarSet(t *testing.T) {
	if err := os.Setenv("PX_DISABLE_TLS", "1"); err != nil {
		t.Fatalf("failed to set the `PX_DISABLE_TLS` environment variable: %v", err)
	}
	c := &Client{}
	opt := WithDisableTLSVerification("cluster.local")
	opt(c)

	assert.True(t, c.disableTLSVerification)

	if err := os.Unsetenv("PX_DISABLE_TLS"); err != nil {
		t.Fatalf("failed to unset the `PX_DISABLE_TLS` environment variable: %v", err)
	}
}
