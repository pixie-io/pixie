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

package scripts_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/utils"
)

func TestCronScript_GetChecksum(t *testing.T) {
	tests := []struct {
		name          string
		mapA          map[string]*cvmsgspb.CronScript
		mapB          map[string]*cvmsgspb.CronScript
		expectedMatch bool
	}{
		{
			name:          "nil",
			mapA:          nil,
			mapB:          nil,
			expectedMatch: true,
		},
		{
			name:          "empty",
			mapA:          make(map[string]*cvmsgspb.CronScript),
			mapB:          make(map[string]*cvmsgspb.CronScript),
			expectedMatch: true,
		},
		{
			name: "one empty",
			mapA: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 10,
					Configs:    "test",
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				},
			},
			mapB:          make(map[string]*cvmsgspb.CronScript),
			expectedMatch: false,
		},
		{
			name: "matching",
			mapA: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 10,
					Configs:    "test",
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				},
				"323e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 100,
					Configs:    "config 2",
					ID:         utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440001"),
				},
				"423e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 2,
					Configs:    "config 3",
					ID:         utils.ProtoFromUUIDStrOrNil("423e4567-e89b-12d3-a456-426655440001"),
				},
			},
			mapB: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 10,
					Configs:    "test",
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				},
				"423e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 2,
					Configs:    "config 3",
					ID:         utils.ProtoFromUUIDStrOrNil("423e4567-e89b-12d3-a456-426655440001"),
				},
				"323e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 100,
					Configs:    "config 2",
					ID:         utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440001"),
				},
			},
			expectedMatch: true,
		},
		{
			name: "slight mismatch",
			mapA: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 10,
					Configs:    "test",
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				},
				"323e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 100,
					Configs:    "config 2",
					ID:         utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440001"),
				},
				"423e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 2,
					Configs:    "config 3",
					ID:         utils.ProtoFromUUIDStrOrNil("423e4567-e89b-12d3-a456-426655440001"),
				},
			},
			mapB: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 11,
					Configs:    "test",
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				},
				"423e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 2,
					Configs:    "config 3",
					ID:         utils.ProtoFromUUIDStrOrNil("423e4567-e89b-12d3-a456-426655440001"),
				},
				"323e4567-e89b-12d3-a456-426655440001": {
					Script:     "px.display()",
					FrequencyS: 100,
					Configs:    "config 2",
					ID:         utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440001"),
				},
			},
			expectedMatch: false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			checksumA, err := scripts.ChecksumFromScriptMap(test.mapA)
			require.NoError(t, err)
			checksumB, err := scripts.ChecksumFromScriptMap(test.mapB)
			require.NoError(t, err)
			assert.Equal(t, test.expectedMatch, checksumA == checksumB)
		})
	}
}
