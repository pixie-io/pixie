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

package k8s_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/utils/shared/k8s"
)

func TestKeyValueStringToMap(t *testing.T) {
	keyValueStringToMapTests := []struct {
		name        string
		str         string
		expectedMap map[string]string
		expectError bool
	}{
		{
			name: "basic",
			str:  "PL_KEY_1=value1,PL_KEY_2=value2",
			expectedMap: map[string]string{
				"PL_KEY_1": "value1",
				"PL_KEY_2": "value2",
			},
			expectError: false,
		},
		{
			name: "comma in value",
			str:  `PL_KEY_1="valuea,valueb",PL_KEY_2=value2`,
			expectedMap: map[string]string{
				"PL_KEY_1": "valuea,valueb",
				"PL_KEY_2": "value2",
			},
			expectError: false,
		},
		{
			name: "equals in value",
			str:  `PL_KEY_1="a=b",PL_KEY_2=value2`,
			expectedMap: map[string]string{
				"PL_KEY_1": "a=b",
				"PL_KEY_2": "value2",
			},
			expectError: false,
		},
		{
			name:        "unquoted comma in value",
			str:         `PL_KEY_1=a,b`,
			expectError: true,
		},
		{
			name:        "unquoted equals in value",
			str:         `PL_KEY_1=a=b`,
			expectError: true,
		},
	}

	for _, tc := range keyValueStringToMapTests {
		t.Run(tc.name, func(t *testing.T) {
			outMap, err := k8s.KeyValueStringToMap(tc.str)

			if tc.expectError {
				require.NotNil(t, err)
				return
			}
			require.Nil(t, err)
			assert.Equal(t, tc.expectedMap, outMap)
		})
	}
}
