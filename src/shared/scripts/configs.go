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

package scripts

// Config represents the configuration for a script. For example: which variables should be pulled in and how.
type Config struct {
	OtelEndpointConfig *OtelEndpointConfig `yaml:"otelEndpointConfig"`
}

// OtelEndpointConfig specifies values that should be filled in for all OTel endpoints in the script.
type OtelEndpointConfig struct {
	URL      string            `yaml:"url"`
	Headers  map[string]string `yaml:"headers"`
	Insecure bool              `yaml:"insecure"`
}
