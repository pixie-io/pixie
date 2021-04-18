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

package live_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/pixie_cli/pkg/live"
)

type tabStop struct {
	Index    int
	Label    string
	Value    string
	HasLabel bool
}

func TestParseInput(t *testing.T) {
	tests := []struct {
		name             string
		input            string
		expectedTabStops []*tabStop
	}{
		{
			name:  "basic",
			input: "${1:run} ${2:svc_name:$0pl/test} ${3}",
			expectedTabStops: []*tabStop{
				{
					Index:    1,
					Label:    "run",
					Value:    "",
					HasLabel: false,
				},
				{
					Index:    2,
					Label:    "svc_name",
					Value:    "$0pl/test",
					HasLabel: true,
				},
				{
					Index:    3,
					Label:    "",
					Value:    "",
					HasLabel: false,
				},
			},
		},
		{
			name:  "empty label",
			input: "${1:run} ${2:svc_name:} ${3}",
			expectedTabStops: []*tabStop{
				{
					Index:    1,
					Label:    "run",
					Value:    "",
					HasLabel: false,
				},
				{
					Index:    2,
					Label:    "svc_name",
					Value:    "",
					HasLabel: true,
				},
				{
					Index:    3,
					Label:    "",
					Value:    "",
					HasLabel: false,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			cmd, err := live.ParseInput(test.input)
			require.NoError(t, err)
			assert.Equal(t, len(test.expectedTabStops), len(cmd.TabStops))

			for i, ts := range cmd.TabStops {
				assert.Equal(t, test.expectedTabStops[i].Index, *ts.Index)
				if test.expectedTabStops[i].Label == "" {
					assert.Nil(t, ts.Label)
				} else {
					assert.Equal(t, test.expectedTabStops[i].Label, *ts.Label)
				}
				if test.expectedTabStops[i].Value == "" {
					assert.Nil(t, ts.Value)
				} else {
					assert.Equal(t, test.expectedTabStops[i].Value, *ts.Value)
				}
				assert.Equal(t, test.expectedTabStops[i].HasLabel, ts.HasLabel)
			}
		})
	}
}
