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

package cvmsgs

var (
	// CronScriptChecksumRequestChannel is the NATS channel to make checksum requests to.
	CronScriptChecksumRequestChannel = "GetCronScriptsCheckSumRequest"
	// CronScriptChecksumResponseChannel is the NATS channel that checksum responses are published to.
	CronScriptChecksumResponseChannel = "GetCronScriptsCheckSumResponse"
	// GetCronScriptsRequestChannel is the NATS channel script requests are sent to.
	GetCronScriptsRequestChannel = "GetCronScriptsRequest"
	// GetCronScriptsResponseChannel is the NATS channel that script responses are published to.
	GetCronScriptsResponseChannel = "GetCronScriptsResponse"
	// CronScriptUpdatesChannel is the NATS channel that any cron script updates are published to.
	CronScriptUpdatesChannel = "CronScriptsUpdates"
	// CronScriptUpdatesResponseChannel is the NATS channel that script updates are published to.
	CronScriptUpdatesResponseChannel = "CronScriptsUpdatesResponse"

	// VizierMetricsChannel is the NATS channel on the cloud side that Vizier metrics are published to.
	VizierMetricsChannel = "VZMetrics"
)
