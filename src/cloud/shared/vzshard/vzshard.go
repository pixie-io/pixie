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

package vzshard

import (
	"encoding/hex"
	"fmt"

	"github.com/gofrs/uuid"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

// SetupFlags install the flag handlers for vizier shards.
func SetupFlags() {
	pflag.Int("vizier_shard_min", 0, "The min vizier shard for this program (inclusive)")
	pflag.Int("vizier_shard_max", 255, "The max vizier shard for this program (inclusive)")
}

func minShard() int {
	return viper.GetInt("vizier_shard_min")
}

func maxShard() int {
	return viper.GetInt("vizier_shard_max")
}

func shardIntToHex(i int) string {
	return fmt.Sprintf("%02x", i)
}

// ShardMin returns the min hex value of the vizier shard.
func ShardMin() string {
	return shardIntToHex(minShard())
}

// ShardMax returns the max hex value of the vizier shard.
func ShardMax() string {
	return shardIntToHex(maxShard())
}

// GenerateShardRange shard range produces the hex values 00-ff for the shards as configured.
func GenerateShardRange() []string {
	min := minShard()
	max := maxShard()
	r := make([]string, max-min+1)
	for i := min; i <= max; i++ {
		r[i-min] = shardIntToHex(i)
	}
	return r
}

// VizierIDToShard provides the shardID for a given vizierID.
func VizierIDToShard(vizierID uuid.UUID) string {
	return hex.EncodeToString(vizierID.Bytes()[uuid.Size-1:])
}

// C2VTopic returns the sharded topic.
func C2VTopic(topic string, vizierID uuid.UUID) string {
	return fmt.Sprintf("c2v.%s.%s", vizierID.String(), topic)
}

// C2VDurableTopic returns the sharded durable topic name.
func C2VDurableTopic(topic string, vizierID uuid.UUID) string {
	return C2VTopic("Durable"+topic, vizierID)
}

// V2CTopic returns the sharded topic name.
func V2CTopic(topic string, vizierID uuid.UUID) string {
	return fmt.Sprintf("v2c.%s.%s.%s", VizierIDToShard(vizierID), vizierID.String(), topic)
}

// V2CDurableTopic returns the sharded durable topic name.
func V2CDurableTopic(topic string, vizierID uuid.UUID) string {
	return V2CTopic("Durable"+topic, vizierID)
}
