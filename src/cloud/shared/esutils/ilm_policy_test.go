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

package esutils_test

import (
	"context"
	"encoding/json"
	"testing"

	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/shared/esutils"
)

func cleanupPolicy(t *testing.T, policyName string) {
	resp, err := elasticClient.XPackIlmDeleteLifecycle().Policy(policyName).Do(context.Background())
	if err != nil {
		t.Logf("ErrorDetails: %v", err.(*elastic.Error).Details)
	}
	require.NoError(t, err)
	require.True(t, resp.Acknowledged)
}

func TestILMPolicyMigrate(t *testing.T) {
	testCases := []struct {
		name               string
		policyName         string
		maxIndexSize       string
		timeBeforeDelete   string
		expectErr          bool
		createBeforeConfig *struct {
			maxIndexSize     string
			timeBeforeDelete string
		}
	}{
		{
			name:               "creates a new policy when one doesn't exist",
			policyName:         "test_new_ilm_policy",
			maxIndexSize:       "50gb",
			timeBeforeDelete:   "1d",
			expectErr:          false,
			createBeforeConfig: nil,
		},
		{
			name:             "updates a policy if the config is different",
			policyName:       "test_ilm_policy_update",
			maxIndexSize:     "50gb",
			timeBeforeDelete: "1d",
			expectErr:        false,
			createBeforeConfig: &struct {
				maxIndexSize     string
				timeBeforeDelete string
			}{
				maxIndexSize:     "25gb",
				timeBeforeDelete: "2d",
			},
		},
		{
			name:               "returns error on invalid max index size",
			policyName:         "test_error_max_index",
			maxIndexSize:       "abcd",
			timeBeforeDelete:   "1d",
			expectErr:          true,
			createBeforeConfig: nil,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			if tc.createBeforeConfig != nil {
				err := esutils.NewILMPolicy(elasticClient, tc.policyName).
					Rollover(&tc.createBeforeConfig.maxIndexSize, nil, nil).
					DeleteAfter(tc.createBeforeConfig.timeBeforeDelete).
					Migrate(context.Background())
				require.NoError(t, err)
			}

			expectedPolicy := esutils.NewILMPolicy(elasticClient, tc.policyName).
				Rollover(&tc.maxIndexSize, nil, nil).
				DeleteAfter(tc.timeBeforeDelete)
			err := expectedPolicy.Migrate(context.Background())
			if tc.expectErr {
				require.NotNil(t, err)
				return
			}
			require.NoError(t, err)
			// Only cleanup if the creation was successful.
			defer cleanupPolicy(t, tc.policyName)

			policyMap, err := elasticClient.XPackIlmGetLifecycle().Policy(tc.policyName).Do(context.Background())
			require.NoError(t, err)
			policyResp, ok := policyMap[tc.policyName]
			require.True(t, ok)

			policyRespBytes, err := json.Marshal(policyResp)
			require.NoError(t, err)

			actualPolicy := esutils.NewILMPolicy(elasticClient, tc.policyName).FromJSONString(string(policyRespBytes))

			assert.Equal(t, expectedPolicy, actualPolicy)
		})
	}
}
