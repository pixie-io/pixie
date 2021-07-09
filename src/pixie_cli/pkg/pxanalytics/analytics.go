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

package pxanalytics

import (
	"fmt"
	"io"
	"net/http"
	"runtime"
	"sync"

	"github.com/gofrs/uuid"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"

	version "px.dev/pixie/src/shared/goversion"
)

var (
	client analytics.Client
	once   sync.Once
)

// A noop client to use if we don't have keys.
type disabledAnalyticsClient struct{}

func (c disabledAnalyticsClient) Enqueue(analytics.Message) error {
	return nil
}
func (c disabledAnalyticsClient) Close() error {
	return nil
}

// Client returns the default analytics client.
func Client() analytics.Client {
	once.Do(func() {
		client = disabledAnalyticsClient{}

		if viper.GetBool("do_not_track") {
			return
		}

		cloudAddr := viper.GetString("cloud_addr")
		resp, err := http.Get(fmt.Sprintf("https://segment.%s/cli-write-key", cloudAddr))

		var analyticsKey []byte

		// TODO(vihang): Remove this once cloud proxy supports the `cli-write-key` path
		// and we are past the revert window for cloud.
		// This is to support a new cli talking to an older version of cloud.
		if resp.StatusCode == 404 && cloudAddr == "withpixie.ai:443" {
			analyticsKey = []byte("ehDrHWhR396KwcAQz0syA8YjwhwLXD1v")
		}

		if err == nil && resp.StatusCode == 200 {
			analyticsKey, err = io.ReadAll(resp.Body)
			resp.Body.Close()
			if err != nil {
				return
			}
		}

		if len(analyticsKey) == 0 {
			return
		}

		client, _ = analytics.NewWithConfig(string(analyticsKey), analytics.Config{
			Endpoint: fmt.Sprintf("https://segment.%s", cloudAddr),
			DefaultContext: &analytics.Context{
				App: analytics.AppInfo{
					Name:    "PX CLI",
					Version: version.GetVersion().ToString(),
					Build:   version.GetVersion().RevisionStatus(),
				},
				OS: analytics.OSInfo{
					Name: runtime.GOOS,
				},
				Extra: map[string]interface{}{
					"sessionID": uuid.Must(uuid.NewV4()).String(),
				},
			},
		})
	})
	return client
}
