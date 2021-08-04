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

package vzshard_test

import (
	"testing"

	"github.com/gofrs/uuid"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/cloud/shared/vzshard"
)

func TestGenerateShardRange(t *testing.T) {
	tests := []struct {
		name string
		min  uint8
		max  uint8

		// Outputs:
		expMinShardID string
		expMaxShardID string

		expShardList []string
	}{
		{
			name: "Shard range from 0-5",
			min:  0,
			max:  5,

			expMinShardID: "00",
			expMaxShardID: "05",

			expShardList: []string{"00", "01", "02", "03", "04", "05"},
		},
		{
			name: "Shard range from 200-255",
			min:  250,
			max:  255,

			expMinShardID: "fa",
			expMaxShardID: "ff",

			expShardList: []string{"fa", "fb", "fc", "fd", "fe", "ff"},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			viper.Set("vizier_shard_min", test.min)
			viper.Set("vizier_shard_max", test.max)

			assert.Equal(t, test.expMinShardID, vzshard.ShardMin())
			assert.Equal(t, test.expMaxShardID, vzshard.ShardMax())
			assert.Equal(t, vzshard.GenerateShardRange(), test.expShardList)
		})
	}
}

func TestVizierIDToShard(t *testing.T) {
	tests := []struct {
		input  string
		output string
	}{
		{
			input:  "",
			output: "00",
		},
		{
			input:  "c5214a44-f04b-48a8-a1d4-a528f2b494d0",
			output: "d0",
		},
		{
			input:  "26d106b2-5493-4709-8f0f-9221490da70b",
			output: "0b",
		},
	}

	for _, test := range tests {
		t.Run(test.input, func(t *testing.T) {
			assert.Equal(t, test.output, vzshard.VizierIDToShard(uuid.FromStringOrNil(test.input)))
		})
	}
}

func TestC2VTopic(t *testing.T) {
	assert.Equal(t, "c2v.c5214a44-f04b-48a8-a1d4-a528f2b494d0.test",
		vzshard.C2VTopic("test", uuid.FromStringOrNil("c5214a44-f04b-48a8-a1d4-a528f2b494d0")))
}

func TestC2VDurableTopic(t *testing.T) {
	assert.Equal(t, "c2v.c5214a44-f04b-48a8-a1d4-a528f2b494d0.Durabletest",
		vzshard.C2VDurableTopic("test", uuid.FromStringOrNil("c5214a44-f04b-48a8-a1d4-a528f2b494d0")))
}

func TestV2CTopic(t *testing.T) {
	assert.Equal(t, "v2c.d0.c5214a44-f04b-48a8-a1d4-a528f2b494d0.test",
		vzshard.V2CTopic("test", uuid.FromStringOrNil("c5214a44-f04b-48a8-a1d4-a528f2b494d0")))
}

func TestV2CDurableTopic(t *testing.T) {
	assert.Equal(t, "v2c.d0.c5214a44-f04b-48a8-a1d4-a528f2b494d0.Durabletest",
		vzshard.V2CDurableTopic("test", uuid.FromStringOrNil("c5214a44-f04b-48a8-a1d4-a528f2b494d0")))
}
