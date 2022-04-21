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

package messagebus

import (
	"fmt"
	"path"

	"github.com/gofrs/uuid"
)

const (
	// agentTopicPrefix is the prefix for messages to specifc agents.
	agentTopicPrefix = "Agent"
	// c2vTopicPrefix is the prefix for all message topics from cloud domain to local NATS domain.
	c2vTopicPrefix = "c2v"
	// v2cTopicPrefix is the prefix for all message topics sent from local NATS to cloud domain.
	v2cTopicPrefix = "v2c"
	// MetricsTopic is the topic name for prometheus metrics being sent from an agent to cloud connector.
	MetricsTopic = "Metrics"
)

// V2CTopic returns the topic used in the Vizier NATS domain to send messages from Vizier to Cloud.
func V2CTopic(topic string) string {
	return fmt.Sprintf("%s.%s", v2cTopicPrefix, topic)
}

// C2VTopic returns the topic used in the Vizier NATS domain to get messages from the Cloud.
func C2VTopic(topic string) string {
	return fmt.Sprintf("%s.%s", c2vTopicPrefix, topic)
}

// AgentUUIDTopic topic used to communicate to a specific agent.
func AgentUUIDTopic(agentID uuid.UUID) string {
	return AgentTopic(agentID.String())
}

// AgentTopic topic used to communicate to a specific agent.
func AgentTopic(agentID string) string {
	return path.Join(agentTopicPrefix, agentID)
}
